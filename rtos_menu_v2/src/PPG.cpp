#include "PPG.h"

namespace {
constexpr uint32_t PPG_TASK_STACK_WORDS = 2048;
StaticTask_t ppgTaskBuffer;
StackType_t ppgTaskStack[PPG_TASK_STACK_WORDS];
}

PPG::PPG(Controls& ctrl) : controls(ctrl) {
    dataMutex = xSemaphoreCreateMutex();
    resetMeasurementState();
}

void PPG::resetMeasurementState() {
    ambientBaseline = 0;
    currentValue = 0;
    lastPPGReadTime = 0;
    bufferIndex = 0;
    emaValue = 0.0f;
    smaIndex = 0;
    smaCount = 0;
    intervalIndex = 0;
    lastPeakMillis = 0;
    signalAboveThreshold = false;
    adjThreshold = 0;
    currentBPM = 0;
    lastAcceptedBPM = 0;
    bpmIsValid = false;
    fingerWasPresent = false;
    lastFingerLostTime = 0;
    ledOn = false;
    lastPeakLedTime = 0;

    memset(ppgBuffer, 0, sizeof(ppgBuffer));
    memset(smaBuffer, 0, sizeof(smaBuffer));
    memset(intervalBuffer, 0, sizeof(intervalBuffer));
}

void PPG::begin() {
    _exit = false;
    running = true;
    currentState = MEASURING;
    menuSelection = 0;
    currentMode = IR;

    digitalWrite(controls.IR_LED_PIN, HIGH);
    digitalWrite(controls.RED_LED_PIN, LOW);
    resetMeasurementState();

    if (taskHandle == nullptr) {
        taskHandle = xTaskCreateStaticPinnedToCore(
            task,
            "PPG Task",
            PPG_TASK_STACK_WORDS,
            this,
            2,
            ppgTaskStack,
            &ppgTaskBuffer,
            1
        );

        if (taskHandle == nullptr) {
            running = false;
            Serial.println("[PPG] static task create FAIL");
            return;
        }

        Serial.println("[PPG] static task create OK");
    } else {
        vTaskResume(taskHandle);
        Serial.println("[PPG] task resumed");
    }
}

void PPG::task(void* pvParameters) {
    PPG* self = static_cast<PPG*>(pvParameters);

    for (;;) {
        if (!self->running) {
            vTaskSuspend(nullptr);
        }

        if (self->_exit) {
            digitalWrite(self->controls.IR_LED_PIN, LOW);
            digitalWrite(self->controls.RED_LED_PIN, LOW);

            self->controls.mutexLed([&]() {
                self->controls.strip.clear();
                self->controls.strip.show();
            });

            self->ledOn = false;
            self->running = false;

            UBaseType_t highWaterWords = uxTaskGetStackHighWaterMark(nullptr);
            Serial.printf(
                "[PPG] task suspended | stack free minimum: %u bytes\n",
                (unsigned int)(highWaterWords * sizeof(StackType_t))
            );

            vTaskSuspend(nullptr);
            continue;
        }

        switch (self->currentState) {
            case MEASURING:
                self->handleLedSwitch();
                self->sampleAndProcessPPG();
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

        self->blinkPeak(false);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void PPG::sampleAndProcessPPG() {
    const unsigned long now = millis();
    if (now - lastPPGReadTime < PPG_READ_INTERVAL_MS) {
        return;
    }
    lastPPGReadTime = now;

    const bool fingerOn = controls.fingerTouched();
    bool peakDetected = false;

    if (fingerOn) {
        const int rawValue = readPPG();

        emaValue = EMA_ALPHA * rawValue + (1.0f - EMA_ALPHA) * emaValue;
        smaBuffer[smaIndex] = static_cast<int>(emaValue);
        smaIndex = (smaIndex + 1) % SMA_SIZE;
        if (smaCount < SMA_SIZE) {
            smaCount++;
        }

        if (smaCount < SMA_SIZE) {
            currentValue = static_cast<int>(emaValue);
        } else {
            int sum = 0;
            for (int i = 0; i < SMA_SIZE; ++i) {
                sum += smaBuffer[i];
            }
            currentValue = sum / SMA_SIZE;
        }

        ppgBuffer[bufferIndex] = currentValue;
        bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;

        adjThreshold = calculateDynamicThreshold();
        peakDetected = detectPeak(currentValue, adjThreshold, now);
        if (peakDetected) {
            blinkPeak(true);
        }

        fingerWasPresent = true;
    } else {
        if (fingerWasPresent) {
            lastFingerLostTime = now;
            fingerWasPresent = false;
        }

        signalAboveThreshold = false;
        lastPeakMillis = 0;
        currentValue = 0;
        emaValue = 0.0f;
        smaIndex = 0;
        smaCount = 0;
        memset(smaBuffer, 0, sizeof(smaBuffer));

        lockData();
        bpmIsValid = false;
        unlockData();
    }

    renderMeasurementScreen(fingerOn);
}

void PPG::renderMeasurementScreen(bool fingerOn) {
    controls.mutexDisplay([&]() {
        auto& display = controls.display;
        display.clearDisplay();

        if (fingerOn) {
            drawPPGCurve(display);
        } else {
            display.setTextSize(1);
            display.setTextColor(SH110X_WHITE);

            if (millis() - lastFingerLostTime < FINGER_LOST_POPUP_MS) {
                display.setCursor(20, 24);
                display.println("Prst odebran");
                display.setCursor(12, 36);
                display.println("Obnovte kontakt");
            } else {
                display.setCursor(24, 30);
                display.println("Prilozte prst");
            }
        }

        displayBPM(display, fingerOn);
        display.display();
    });
}

bool PPG::detectPeak(int value, int threshold, unsigned long now) {
    bool peakDetected = false;

    if (!signalAboveThreshold && value > threshold) {
        signalAboveThreshold = true;
    }

    if (signalAboveThreshold && value < threshold) {
        const unsigned long interval = now - lastPeakMillis;
        if (lastPeakMillis != 0 && interval >= MIN_BEAT_INTERVAL_MS) {
            calculateBPM(interval);
            peakDetected = true;
        }
        lastPeakMillis = now;
        signalAboveThreshold = false;
    }

    return peakDetected;
}

void PPG::handleStateSwitch() {
    if (currentState == MEASURING && controls.rightPressed) {
        currentState = MENU;
        menuSelection = 0;
        controls.rightPressed = false;
        drawMenu();
        return;
    }

    if (currentState != MENU) {
        return;
    }

    if (controls.leftPressed) {
        menuSelection = (menuSelection - 1 + menuItemCount) % menuItemCount;
        controls.leftPressed = false;
        drawMenu();
    }

    if (controls.rightPressed) {
        menuSelection = (menuSelection + 1) % menuItemCount;
        controls.rightPressed = false;
        drawMenu();
    }

    if (!controls.FingerTouchedFlag()) {
        return;
    }

    switch (menuSelection) {
        case MENU_CALIBRATION:
            currentState = CALIBRATION;
            break;

        case MENU_AUTOZOOM:
            autoZoomEnabled = !autoZoomEnabled;
            drawMenu();
            break;

        case MENU_BACK:
            currentState = MEASURING;
            break;

        case MENU_EXIT:
            _exit = true;
            break;
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
    lockData();

    intervalBuffer[intervalIndex] = interval;
    intervalIndex = (intervalIndex + 1) % INTERVAL_BUFFER_SIZE;

    unsigned long sum = 0;
    int count = 0;
    for (int i = 0; i < INTERVAL_BUFFER_SIZE; ++i) {
        if (intervalBuffer[i] > 0) {
            sum += intervalBuffer[i];
            count++;
        }
    }

    if (count > 0) {
        const unsigned long avgInterval = sum / count;
        if (avgInterval > 0) {
            const int candidateBpm = 60000 / avgInterval;
            if (isBpmPlausible(candidateBpm)) {
                currentBPM = candidateBpm;
                lastAcceptedBPM = candidateBpm;
                bpmIsValid = true;
            }
        }
    }

    unlockData();
}

bool PPG::isBpmPlausible(int bpm) const {
    if (bpm < BPM_MIN_VALID || bpm > BPM_MAX_VALID) {
        return false;
    }

    if (!bpmIsValid) {
        return true;
    }

    return abs(bpm - lastAcceptedBPM) <= BPM_MAX_JUMP;
}

void PPG::calibrateSensor() {
    digitalWrite(controls.IR_LED_PIN, LOW);
    digitalWrite(controls.RED_LED_PIN, LOW);
    vTaskDelay(pdMS_TO_TICKS(300));

    constexpr int totalSamples = 100;
    long sum = 0;

    for (int i = 0; i < totalSamples; ++i) {
        sum += analogRead(controls.PPG_PIN);

        const int progressWidth = map(i + 1, 0, totalSamples, 0, 100);
        controls.mutexDisplay([&]() {
            auto& display = controls.display;
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SH110X_WHITE);
            display.setCursor(34, 5);
            display.println("Fotosenzor");
            display.setCursor(0, 20);
            display.println("Kalibrace senzoru...");
            display.drawRect(13, 34, 102, 12, SH110X_WHITE);
            display.fillRect(14, 35, progressWidth, 10, SH110X_WHITE);
            display.display();
        });

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    ambientBaseline = sum / totalSamples;
    resetMeasurementState();
    ambientBaseline = sum / totalSamples;
    currentMode = currentMode == RED ? RED : IR;

    controls.mutexDisplay([&]() {
        auto& display = controls.display;
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(0, 24);
        display.println("Kalibrace dokoncena");
        display.display();
    });

    vTaskDelay(pdMS_TO_TICKS(700));
}

int PPG::readPPG() {
    return analogRead(controls.PPG_PIN) - ambientBaseline;
}

void PPG::drawPPGCurve(Adafruit_SH1106G& display) {
    constexpr int topMargin = 10;
    constexpr int bottomMargin = 5;

    const int drawTop = topMargin;
    const int drawBottom = DISPLAY_HEIGHT - bottomMargin - 1;
    const int availableHeight = drawBottom - drawTop + 1;
    const int axisY = drawTop + availableHeight / 2;

    const float halfRange = (availableHeight - 2) / 2.0f;

    constexpr int manualAmplitude = 1500;
    constexpr float minZoom = 0.2f;
    constexpr float maxZoom = 20.0f;

    // Menší padding = větší křivka
    constexpr float peakPadding = 0.95f;

    // Tvrdý strop amplitudy pro autozoom
    // Vyšší číslo = plošší křivka
    // Nižší číslo = agresivnější přiblížení
    constexpr float maxAutoAmplitude = 260.0f;

    // Minimální amplituda kvůli stabilitě
    constexpr float minAutoAmplitude = 60.0f;

    const int windowSize = min(BUFFER_SIZE, 80);
    if (windowSize < 2) {
        return;
    }

    const int baseIdx = (bufferIndex - windowSize + BUFFER_SIZE) % BUFFER_SIZE;

    float baseline = 0.0f;
    float zoom = 1.0f;

    if (autoZoomEnabled) {
        // Pro autozoom ber jen novější část okna, ať staré extrémy nekazí škálování
        const int zoomWindow = min(windowSize, 32);
        const int zoomBaseIdx = (bufferIndex - zoomWindow + BUFFER_SIZE) % BUFFER_SIZE;

        int minVal = ppgBuffer[zoomBaseIdx];
        int maxVal = ppgBuffer[zoomBaseIdx];
        long sum = 0;

        for (int i = 0; i < zoomWindow; ++i) {
            const int idx = (zoomBaseIdx + i) % BUFFER_SIZE;
            const int val = ppgBuffer[idx];

            if (val < minVal) minVal = val;
            if (val > maxVal) maxVal = val;
            sum += val;
        }

        baseline = static_cast<float>(sum) / zoomWindow;

        const float upperAmp = fabsf(maxVal - baseline);
        const float lowerAmp = fabsf(baseline - minVal);
        float peakAmplitude = max(upperAmp, lowerAmp);

        // Drž amplitudu v rozumném rozsahu
        if (peakAmplitude < minAutoAmplitude) peakAmplitude = minAutoAmplitude;
        if (peakAmplitude > maxAutoAmplitude) peakAmplitude = maxAutoAmplitude;

        zoom = constrain((halfRange * peakPadding) / peakAmplitude, minZoom, maxZoom);
    } else {
        baseline = 0.0f;
        zoom = constrain(halfRange / manualAmplitude, minZoom, maxZoom);
    }

    const float xStep = static_cast<float>(DISPLAY_WIDTH - 1) / static_cast<float>(windowSize - 1);

    for (int i = 1; i < windowSize; ++i) {
        const int prevIndex = (baseIdx + i - 1) % BUFFER_SIZE;
        const int currIndex = (baseIdx + i) % BUFFER_SIZE;

        const float y1f = axisY - (ppgBuffer[prevIndex] - baseline) * zoom;
        const float y2f = axisY - (ppgBuffer[currIndex] - baseline) * zoom;

        const int x1 = static_cast<int>(round((i - 1) * xStep));
        const int x2 = static_cast<int>(round(i * xStep));
        const int y1 = constrain(static_cast<int>(round(y1f)), drawTop, drawBottom);
        const int y2 = constrain(static_cast<int>(round(y2f)), drawTop, drawBottom);

        display.drawLine(x1, y1, x2, y2, SH110X_WHITE);
    }
}

void PPG::displayBPM(Adafruit_SH1106G& display, bool fingerOn) {
    int bpmToShow = 0;
    bool valid = false;

    lockData();
    bpmToShow = currentBPM;
    valid = bpmIsValid;
    unlockData();

    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.print("BPM:");
    display.setCursor(25, 0);

    if (!fingerOn || !valid) {
        display.print("...");
    } else {
        display.print(bpmToShow);
    }

    display.setCursor(96, 0);
    display.print(currentMode == IR ? "IR" : "RED");
}

int PPG::calculateDynamicThreshold() const {
    constexpr int window = 16;
    constexpr float thresholdFactor = 0.6f;

    int maxVal = ppgBuffer[(bufferIndex - 1 + BUFFER_SIZE) % BUFFER_SIZE];
    int minVal = maxVal;

    for (int i = 0; i < window; ++i) {
        const int idx = (bufferIndex - 1 - i + BUFFER_SIZE) % BUFFER_SIZE;
        const int val = ppgBuffer[idx];
        maxVal = max(maxVal, val);
        minVal = min(minVal, val);
    }

    if (maxVal <= minVal) {
        return minVal;
    }

    return minVal + static_cast<int>((maxVal - minVal) * thresholdFactor);
}

void PPG::drawMenu() {
    controls.mutexDisplay([&]() {
        auto& display = controls.display;
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(0, 0);
        display.println("     NASTAVENI     ");

        for (int i = 0; i < menuItemCount; ++i) {
            display.setCursor(0, i * 10 + 12);
            display.print((i == menuSelection) ? "> " : "  ");
            display.print(menuItems[i]);

            if (i == MENU_AUTOZOOM) {
                display.setCursor(92, i * 10 + 12);
                display.print(autoZoomEnabled ? "ON" : "OFF");
            }
        }

        display.display();
    });
}

void PPG::blinkPeak(bool peak) {
    const unsigned long now = millis();

    if (peak && !ledOn) {
        controls.mutexLed([&]() {
            controls.strip.setPixelColor(0, 0, 255, 0);
            controls.strip.show();
        });
        lastPeakLedTime = now;
        ledOn = true;
        return;
    }

    if (ledOn && now - lastPeakLedTime >= BLINK_DURATION_MS) {
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
    return running;
}

void PPG::web_ppg() {
    web.server().on("/ppg/update", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (_exit) {
            request->send(503, "application/json", "{}");
            return;
        }

        JsonDocument doc;
        lockData();
        doc["bpm"] = bpmIsValid ? currentBPM : 0;
        doc["mode"] = (currentMode == IR) ? "IR" : "RED";
        unlockData();

        String json;
        serializeJson(doc, json);

        AsyncWebServerResponse* response = request->beginResponse(200, "application/json", json);
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Cache-Control", "no-cache");
        request->send(response);
    });

    web.server().on("/ppg/toggle-led", HTTP_GET, [this](AsyncWebServerRequest* request) {
        currentMode = (currentMode == IR) ? RED : IR;
        request->send(200, "text/plain", "ok");
    });

    web.server().on("/ppg/autozoom", HTTP_GET, [this](AsyncWebServerRequest* request) {
        autoZoomEnabled = !autoZoomEnabled;
        request->send(200, "text/plain", "ok");
    });

    web.server().on("/ppg/calibrate", HTTP_GET, [this](AsyncWebServerRequest* request) {
        currentState = CALIBRATION;
        request->send(200, "text/plain", "ok");
    });

    web.server().on("/ppg/exit", HTTP_GET, [this](AsyncWebServerRequest* request) {
        _exit = true;
        request->send(200, "text/plain", "ok");
    });
}

void PPG::lockData() {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
}

void PPG::unlockData() {
    xSemaphoreGive(dataMutex);
}