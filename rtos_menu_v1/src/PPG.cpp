#include "PPG.h"

PPG::PPG(Controls& ctrl)
: controls(ctrl),
  ambientBaseline(0), currentValue(0), lastPPGReadTime(0),
  bufferIndex(0), emaValue(0), smaIndex(0),
  lastPeakTime(0), currentBPM(0), bpmIsValid(false),
  menuActive(false), menuSelection(0), currentMode(IR),
  currentState(MEASURING), adjThreshold(0),
  touch_baseline(20), autoZoomEnabled(false),
  intervalIndex(0), lastPeakTime_LED(0),
  ledOn(false), peakFlag(false),
  lastBpmSendTime(0),
  lastSentBpm(0), smaCount(0)
{
    memset(ppgBuffer, 0, sizeof(ppgBuffer));
    memset(smaBuffer, 0, sizeof(smaBuffer));
    memset(intervalBuffer, 0, sizeof(intervalBuffer));
    dataMutex = xSemaphoreCreateMutex();
}

void PPG::begin() {
    if (taskHandle != nullptr) return;

    _exit = false;

    xTaskCreatePinnedToCore(
        task,
        "PPG Task",
        8192,
        this,
        2,
        &taskHandle,
        1
    );
}

void PPG::task(void *pvParameters) {
    PPG* self = static_cast<PPG*>(pvParameters);

    for (;;) {

        if (self->_exit) {

            self->controls.mutexLed([&]() {
                self->controls.strip.clear();
                self->controls.strip.show();
            });

            break;
        }

        self->blinkPeak(self->peakFlag);

        switch (self->currentState) {

            case MEASURING:
                self->sampleAndProcessPPG();
                self->handleLedSwitch();
                self->handleStateSwitch();
                break;

            case MENU:
                self->handleStateSwitch();
                break;

            case CALIBRATION:
                self->calibrateSensor();
                self->currentState = MEASURING;
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    self->taskHandle = NULL;
    vTaskDelete(NULL);
}

void PPG::sampleAndProcessPPG() {
    if (millis() - lastPPGReadTime < ppgReadInterval) return;
    lastPPGReadTime = millis();

    bool fingerOn = controls.fingerTouched();

    controls.mutexDisplay([&]() {

        auto& display = controls.display;
        display.clearDisplay();

        if (fingerOn) {
            int rawValue = ReadPPG();

            // EMA filtr
            emaValue = alpha * rawValue + (1 - alpha) * emaValue;

            // SMA buffer
            smaBuffer[smaIndex] = (int)emaValue;
            smaIndex = (smaIndex + 1) % 5;

            if (smaCount < 5) smaCount++;

            // SMA výpočet
            if (smaCount < 5) {
                currentValue = (int)emaValue;
            } else {
                int smaSum = 0;
                for (int i = 0; i < 5; i++) {
                    smaSum += smaBuffer[i];
                }
                currentValue = smaSum / 5;
            }

            // FIFO buffer
            ppgBuffer[bufferIndex] = currentValue;
            bufferIndex = (bufferIndex + 1) % bufferSize;

            // Dynamický threshold
            adjThreshold = calculateDynamicThreshold();

            // Peak detection
            peakFlag = detectPeak(currentValue, adjThreshold);

            // Kreslení PPG
            drawPPGCurve();

        } else {
            display.setTextSize(1);
            display.setTextColor(SH110X_WHITE);
            display.setCursor(25, SCREEN_HEIGHT / 2 - 3);
            display.println("Prilozte prst");
        }

        // BPM zobrazení
        displayBPM();
        display.display();
    });
}

bool PPG::detectPeak(int currentValue, int threshold) {
    static bool aboveThreshold = false;
    static unsigned long lastPeakMillis = 0;

    bool peakDetected = false;
    unsigned long now = millis();

    // Rising edge → vstup nad threshold
    if (!aboveThreshold && currentValue > threshold) {
        aboveThreshold = true;
    }

    // Falling edge → návrat pod threshold = peak
    if (aboveThreshold && currentValue < threshold) {

        unsigned long interval = now - lastPeakMillis;

        if (interval >= minBeatInterval) {
            peakDetected = true;
            calculateBPM(interval);
            lastPeakMillis = now;
        }

        aboveThreshold = false;
    }

    return peakDetected;
}

void PPG::handleStateSwitch() {

    // --- Otevření menu z MEASURING ---
    if (currentState == MEASURING && controls.rightPressed) {
        currentState = MENU;
        menuSelection = 0;
        menuActive = true;
        drawMenu();
        controls.rightPressed = false;
    }

    // --- Menu není aktivní → nic nedělej ---
    if (currentState != MENU || !menuActive) return;

    // --- Navigace v menu ---
    if (controls.leftPressed) {
        menuSelection = (menuSelection - 1 + menuItemCount) % menuItemCount;
        drawMenu();
        controls.leftPressed = false;
    }

    if (controls.rightPressed) {
        menuSelection = (menuSelection + 1) % menuItemCount;
        drawMenu();
        controls.rightPressed = false;
    }

    // --- Potvrzení volby touch senzorem ---
    if (controls.FingerTouchedFlag()) {

        switch (menuSelection) {

            case MENU_CALIBRATION:
                currentState = CALIBRATION;
                menuActive = false;
                break;

            case MENU_AUTOZOOM:
                autoZoomEnabled = !autoZoomEnabled;
                drawMenu();
                break;

            case MENU_BACK:
                currentState = MEASURING;
                menuActive = false;
                controls.display.clearDisplay();
                break;

            case MENU_EXIT:
                _exit = true;
                break;
        }
    }
}

void PPG::handleLedSwitch() {

    if (controls.leftPressed) {
        currentMode = (currentMode == IR) ? RED : IR;
        controls.leftPressed = false;
    }

    digitalWrite(controls.IR_LED_PIN, currentMode == IR ? HIGH : LOW);
    digitalWrite(controls.RED_LED_PIN, currentMode == RED ? HIGH : LOW);
}

void PPG::calculateBPM(unsigned long interval) {
    // Uloží aktuální interval do kruhového bufferu
    intervalBuffer[intervalIndex] = interval;
    intervalIndex = (intervalIndex + 1) % intervalBufferSize;

    // Spočítáme průměr intervalů (ne nula)
    unsigned long sum = 0;
    int validCount = 0;

    for (int i = 0; i < intervalBufferSize; i++) {
        if (intervalBuffer[i] > 0) {
            sum += intervalBuffer[i];
            validCount++;
        }
    }

    if (validCount == 0) {
        bpmIsValid = false;
        return;
    }

    unsigned long avgInterval = sum / validCount;
    int bpm = 60000 / avgInterval;

    // omezení na realistické BPM
    bpmIsValid = (bpm >= 40 && bpm <= 180);
    if (bpmIsValid){
        currentBPM = bpm;
        hrvValue = calculateHRV();
    }
}

void PPG::calibrateSensor() {

    // Vypneme LED
    digitalWrite(controls.IR_LED_PIN, LOW);
    digitalWrite(controls.RED_LED_PIN, LOW);
    delay(500);

    const int totalSamples = 100;
    long sum = 0;

    controls.mutexDisplay([&]() {

        auto& display = controls.display;

        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(34, 5);
        display.println("Fotosenzor");

        display.setCursor(0, 20);
        display.println("Kalibrace senzoru...");
        display.display();

        for (int i = 0; i < totalSamples; i++) {
            sum += analogRead(controls.PPG_PIN);

            int progressWidth = map(i, 0, totalSamples - 1, 0, 100);
            display.fillRect(14, 35, progressWidth, 10, SH110X_WHITE);
            display.display();

            delay(20);
        }

        ambientBaseline = sum / totalSamples;

        display.clearDisplay();
        display.setCursor(0, 20);
        display.println("Kalibrace dokoncena!");
        display.display();

        delay(1000);
    });
}

int PPG::ReadPPG() {
  return analogRead(controls.PPG_PIN) - ambientBaseline;
}

void PPG::drawPPGCurve() {
    auto& display = controls.display;

    // --- Základní rozložení ---
    int topMargin = 10;
    int bottomMargin = 5;
    int availableHeight = height - topMargin - bottomMargin;
    int axisY = availableHeight / 2; // baseline uprostřed

    // manual parametry (upravené pro stabilní a výrazné zobrazení)
    const float manual_baseline = 0;   // ReadPPG už vrací hodnotu relativní k ambientBaseline
    const int manualAmplitude = 1500;      // očekávaná amplituda v manual režimu
    const float manualZoomFactor = 1;  // drobné zvýraznění v manuálu

    // použijeme okno posledních vzorků pro lepší rozlišení
    const int windowSize = min(bufferSize, 80);
    int baseIdx = (bufferIndex - windowSize + bufferSize) % bufferSize;

    float zoom;
    float ppgBaseline;
    float manual_zoom;
    float auto_zoom;

    if (autoZoomEnabled) {
        // --- AUTO režim: dynamická baseline a zoom podle rozsahu v okně ---
        int minVal = ppgBuffer[baseIdx];
        int maxVal = minVal;
        long sum_auto = 0;
        for (int i = 0; i < windowSize; ++i) {
            int idx = (baseIdx + i) % bufferSize;
            int val = ppgBuffer[idx];
            if (val < minVal) minVal = val;
            if (val > maxVal) maxVal = val;
            sum_auto += val;
        }
        ppgBaseline = (float)sum_auto / windowSize;
        int dynamicAmplitude = max(1, maxVal - minVal);
        auto_zoom = (float)availableHeight / (float)dynamicAmplitude;
        // omezíme zoom, aby nebyl extrémní
        const float minZoom = 0.5f;
        const float maxZoom = 12.0f;
        zoom = constrain(auto_zoom, minZoom, maxZoom);
    } else {
        // --- MANUAL režim: stabilní baseline kolem 0 a fixní škála ---
         ppgBaseline = 0; // PPG hodnota je relativní k 0
         manual_zoom = manualZoomFactor * ((float)availableHeight / (float)manualAmplitude);
         const float minZoom = 0.5f;
         const float maxZoom = 12.0f;
         zoom = constrain(manual_zoom, minZoom, maxZoom);
    }

    // --- Kreslení křivky pouze přes windowSize bodů ---
    float xStep = (float)width / (float)(windowSize - 1);
    for (int i = 1; i < windowSize; ++i) {
        int prevIndex = (baseIdx + i - 1) % bufferSize;
        int currentIndex = (baseIdx + i) % bufferSize;

        float y1f = axisY - (ppgBuffer[prevIndex] - ppgBaseline) * zoom;
        float y2f = axisY - (ppgBuffer[currentIndex] - ppgBaseline) * zoom;

        int y1 = constrain((int)round(y1f), topMargin, height - bottomMargin - 1);
        int y2 = constrain((int)round(y2f), topMargin, height - bottomMargin - 1);
        int x1 = (int)round((i - 1) * xStep);
        int x2 = (int)round(i * xStep);

        display.drawLine(x1, y1, x2, y2, SH110X_WHITE);
    }
}

void PPG::displayBPM() {
    auto& display = controls.display;

    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);

    bool finger = controls.fingerTouched();

    display.setCursor(bpmX, bpmY);
    display.print("BPM:");
    display.setCursor(bpmX + 25, bpmY);

    if (!bpmIsValid || !finger) {
        display.print("...");
    } else {
        display.print(currentBPM);

        unsigned long now = millis();

        if (now - lastBpmSendTime >= 1000) {
            lastBpmSendTime = now;

            if (currentBPM != lastSentBpm) {
                SerialBT.printf("BPM:%d\n", currentBPM);
                lastSentBpm = currentBPM;
            }
        }
    }

    display.setCursor(bpmX + 100, bpmY);
    display.print(currentMode == IR ? "IR" : "RED");
}

int PPG::calculateDynamicThreshold() {
    const int window = 16;
    const float thresholdFactor = 0.6f;

    static float smoothed = 0;

    int maxVal = ppgBuffer[bufferIndex];
    int minVal = ppgBuffer[bufferIndex];

    for (int i = 0; i < window; i++) {
        int idx = (bufferIndex - i + bufferSize) % bufferSize;
        int val = ppgBuffer[idx];

        if (val == 0) continue;

        maxVal = max(maxVal, val);
        minVal = min(minVal, val);
    }

    if (maxVal == minVal) {
        return (int)smoothed;
    }

    int newThreshold = minVal + (maxVal - minVal) * thresholdFactor;

    smoothed = alpha * newThreshold + (1 - alpha) * smoothed;

    return (int)smoothed;
}

void PPG::drawMenu() {

    controls.display.clearDisplay();
    controls.display.setTextSize(1);
    controls.display.setTextColor(SH110X_WHITE);
    controls.display.setCursor(0, 0);
    controls.display.println("     NASTAVENI     ");

    for (int i = 0; i < menuItemCount; i++) {
        controls.display.setCursor(0, i * 10 + 10);

        controls.display.print((i == menuSelection) ? "> " : "  ");
        controls.display.print(menuItems[i]);

        // ON/OFF vedle Autozoom přes enum index
        if (i == MENU_AUTOZOOM) {
            controls.display.setCursor(90, i * 10 + 10);
            controls.display.print(autoZoomEnabled ? "ON" : "OFF");
        }
    }

    controls.display.display();
}

void PPG::blinkPeak(bool peak) {
    unsigned long now = millis();

    if (peak && !ledOn) {

        controls.mutexLed([&]() {
            controls.strip.setPixelColor(0, 0, 255, 0);
            controls.strip.show();
        });

        lastPeakTime_LED = now;
        ledOn = true;
        return;
    }

    if (ledOn && now - lastPeakTime_LED >= blinkDuration) {

        controls.mutexLed([&]() {
            controls.strip.setPixelColor(0, 0, 0, 0);
            controls.strip.show();
        });

        ledOn = false;
    }
}

bool PPG::shouldExit() {
    return _exit;
}

bool PPG::isRunning() {
    return taskHandle != NULL;
}

float PPG::calculateHRV() {
    const int N = intervalBufferSize;
    float sumSq = 0;
    int count = 0;

    for (int i = 1; i < N; i++) {
        if (intervalBuffer[i] == 0 || intervalBuffer[i-1] == 0) continue;

        float diff = (float)intervalBuffer[i] - (float)intervalBuffer[i-1];
        sumSq += diff * diff;
        count++;
    }

    if (count == 0) return 0;

    return sqrt(sumSq / count);
}

void PPG::web_ppg() {

    web.server().on("/ppg/update", HTTP_GET, [this](AsyncWebServerRequest *request) {

        if (_exit) {
            request->send(503, "application/json", "{}");
            return;
        }

        JsonDocument doc;

        lockData();

        doc["bpm"] = currentBPM;
        doc["hrv"] = hrvValue;

        JsonArray rr = doc["rr"].to<JsonArray>();
        for (int i = 0; i < intervalBufferSize; i++) {
            rr.add(intervalBuffer[i]);
        }

        unlockData();

        String json;
        serializeJson(doc, json);

        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Cache-Control", "no-cache");

        request->send(response);
    });
}

void PPG::lockData() {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
}

void PPG::unlockData() {
    xSemaphoreGive(dataMutex);
}