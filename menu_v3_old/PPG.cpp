#include "PPG.h"
#include "settings.h"

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
  lastSentBpm(0)
{
    memset(ppgBuffer, 0, sizeof(ppgBuffer));
    memset(smaBuffer, 0, sizeof(smaBuffer));
    memset(intervalBuffer, 0, sizeof(intervalBuffer));
}

void PPG::start() {
    auto& display = controls.display;
    display.clearDisplay();
    display.display();

    calibrateTouch();
    calibrateSensor();

    currentMode = IR;
}

void PPG::loop() {
    blinkPeak(peakFlag);

    switch(currentState) {
        case MEASURING:
            sampleAndProcessPPG();
            handleLedSwitch();
            handleStateSwitch();
            break;

        case MENU:
            handleStateSwitch();
            break;

        case CALIBRATION:
            calibrateSensor();
            currentState = MEASURING;
            break;
    }
}

void PPG::sampleAndProcessPPG() {
    auto& display = controls.display;
    if (millis() - lastPPGReadTime < ppgReadInterval) return;
    lastPPGReadTime = millis();

    bool fingerOn = fingerTouched(); // jediná funkce pro detekci prstu
    display.clearDisplay();

    if (fingerOn) {
        int rawValue = ReadPPG();
        Serial.println(rawValue);

        // EMA filtr
        emaValue = alpha * rawValue + (1 - alpha) * emaValue;

        // SMA buffer
        smaBuffer[smaIndex] = (int)emaValue;
        smaIndex = (smaIndex + 1) % 5;

        int smaSum = 0;
        for (int i = 0; i < 5; i++) smaSum += smaBuffer[i];
        currentValue = smaSum / 5;

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
}

void PPG::handleStateSwitch() {
    auto& display = controls.display;
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
    if (fingerMenuPressed()) {
    const char* item = menuItems[menuSelection];

    if (strcmp(item, "Kalibrace") == 0) {
        currentState = CALIBRATION;
        menuActive = false;
    }
    else if (strcmp(item, "Autozoom") == 0) {
        autoZoomEnabled = !autoZoomEnabled;
        drawMenu();
    }
    else if (strcmp(item, "Zpet") == 0) {
        currentState = MEASURING;
        menuActive = false;
        display.clearDisplay();
    }
    else if (strcmp(item, "Konec") == 0) {
        _exit = true;
    }
    }
}

void PPG::handleLedSwitch() {
    static bool prevButtonState = HIGH;

    // přečteme stav tlačítka z Controls
    bool buttonState = controls.leftPressed;

    // detekce přepnutí (falling edge)
    if (prevButtonState == HIGH && buttonState == LOW) {
        // přepnutí módu
        currentMode = (currentMode == IR) ? RED : IR;
    }

    // uložíme stav pro příště
    prevButtonState = buttonState;

    // reset tlačítka v Controls
    controls.leftPressed = false;

    // aktualizace LED podle aktuálního módu
    digitalWrite(controls.IR_LED_PIN, currentMode == IR ? HIGH : LOW);
    digitalWrite(controls.RED_LED_PIN, currentMode == RED ? HIGH : LOW);
}

bool PPG::detectPeak(int currentValue, int threshold) {
    // statické proměnné si pamatují stav mezi voláními
    static int prevValue = 0;
    static unsigned long lastPeakMillis = 0;

    bool peakDetected = false;

    // Detekce peak: hodnota klesá po překročení threshold
    if (prevValue > threshold && currentValue < prevValue) {
        unsigned long now = millis();
        unsigned long interval = now - lastPeakMillis;

        // respektujeme minimální interval mezi tepy
        if (interval >= minBeatInterval) {
            peakDetected = true;
            calculateBPM(interval);
            lastPeakMillis = now;
        }
    }

    prevValue = currentValue;
    return peakDetected;
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
    if (bpmIsValid) currentBPM = bpm;
}

void PPG::calibrateSensor() {
    auto& display = controls.display;
    // Vypneme LED
    digitalWrite(controls.IR_LED_PIN, LOW);
    digitalWrite(controls.RED_LED_PIN, LOW);
    delay(500);

    const int totalSamples = 100;
    long sum = 0;

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

        // progress bar 0–100
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

    display.setCursor(bpmX, bpmY);
    display.print("BPM:");
    display.setCursor(bpmX + 25, bpmY);
    if (bpmIsValid && fingerTouched()) {
    display.print(currentBPM);

    unsigned long now = millis();

    // pošli jen jednou za 1000 ms
    if (now - lastBpmSendTime >= 1000) {
        lastBpmSendTime = now;

        // volitelně: pošli jen pokud se změnila hodnota
        if (currentBPM != lastSentBpm) {
           // Serial.print("BPM: ");
           // Serial.println(currentBPM);
           // SerialBT.printf("BPM:%d\n", currentBPM);

            lastSentBpm = currentBPM;
        }
    }
    } else {
     display.print("...");
    }

     display.setCursor(bpmX + 100, bpmY);
     display.print(currentMode == IR ? "IR" : "RED");
}

int PPG::calculateDynamicThreshold() {
    const int window = 16;  // sliding window
    static float smoothed = 0;

    int maxVal = ppgBuffer[bufferIndex];
    int minVal = ppgBuffer[bufferIndex];

    // najdi min a max v okně
    for (int i = 0; i < window; i++) {
        int idx = (bufferIndex - i + bufferSize) % bufferSize;
        maxVal = max(maxVal, ppgBuffer[idx]);
        minVal = min(minVal, ppgBuffer[idx]);
    }

    int newThreshold = minVal + (maxVal - minVal) * 0.6f;

    // EMA pro hladší adaptaci
    smoothed = alpha * newThreshold + (1 - alpha) * smoothed;

    return (int)smoothed;
}

void PPG::drawMenu() {
    auto& display = controls.display;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println("==== NASTAVENI ====");

    for (int i = 0; i < menuItemCount; i++) {
        display.setCursor(0, i * 10 + 10);
        display.print((i == menuSelection) ? "> " : "  ");
        display.print(menuItems[i]);

        // ON/OFF vedle Autozoom
        if (strcmp(menuItems[i], "Autozoom") == 0) {
            display.setCursor(90, i * 10 + 10);
            display.print(autoZoomEnabled ? "ON" : "OFF");
        }
    }

    display.display();
}

void PPG::blinkPeak(bool peak) {
    unsigned long now = millis();

    if (peak) {
        controls.strip.setPixelColor(0, 0, 255, 0); // modrá
        controls.strip.show();
        lastPeakTime_LED = now;
        ledOn = true;
        return; // pokud je peak, hned blikni a skonči
    }

    // zhasnutí po blinkDuration
    if (ledOn && now - lastPeakTime_LED >= blinkDuration) {
        controls.strip.setPixelColor(0, 0, 0, 0); // zhasni
        controls.strip.show();
        ledOn = false;
    }
}

void PPG::calibrateTouch() {
    long sum = 0;
    const int samples = 30;

    for (int i = 0; i < samples; i++) {
        sum += touchRead(controls.TOUCH_PIN);
        delay(10);
    }

    touch_baseline = sum / samples;
}

bool PPG::fingerTouched() {
    static bool state = false;
    static unsigned long lastChangeTime = 0;

    int t = touchRead(controls.TOUCH_PIN);
    int threshold = max(5, touch_baseline - 5);

    bool currentlyTouched = t < threshold;

    if (currentlyTouched != state) {
        if (lastChangeTime == 0) lastChangeTime = millis();
        if (millis() - lastChangeTime >= fingerHoldTime) {
            state = currentlyTouched;
            lastChangeTime = 0;
        }
    } else {
        lastChangeTime = 0;
    }

    return state;
}

bool PPG::fingerMenuPressed() {
    static bool prevMenuState = false;
    bool currentMenuState = fingerTouched(); // drží prst při měření
    bool pressed = (!prevMenuState && currentMenuState); // pouze první přiložení
    prevMenuState = currentMenuState;
    return pressed;
}

bool PPG::shouldExit() {
    return _exit;
}

void PPG::begin() {
    controls.strip.setPixelColor(0, 0, 0, 0); // zhasnutí LED
    controls.strip.show();
    _exit = false;
    currentState = MEASURING;
    currentMode = IR;
}