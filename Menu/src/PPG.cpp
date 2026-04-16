#include "PPG.h"

namespace {
constexpr uint32_t PPG_TASK_STACK_WORDS = 2048;
StaticTask_t ppgTaskBuffer;
StackType_t ppgTaskStack[PPG_TASK_STACK_WORDS];
}

PPG::PPG(Controls& ctrl, Settings& settingsRef)
    : controls(ctrl), settings(settingsRef) {
    dataMutex = xSemaphoreCreateMutex();
    resetMeasurementState();
}

void PPG::resetMeasurementState() {
    ambientBaseline = 0;
    currentValue = 0;
    lastPPGReadTime = 0;
    bufferIndex = 0;
    emaValue = 0.0f;
    intervalIndex = 0;
    lastPeakMillis = 0;
    signalAboveThreshold = false;
    adjThreshold = 0;
    currentBPM = 0;
    lastAcceptedBPM = 0;
    bpmIsValid = false;
    ledOn = false;
    lastPeakLedTime = 0;
    displayBaselineSum = 0;
    displayBaselineCounter = 0;
    displayBaseline = 0;
    displayBaselineLocked = false;

    memset(ppgBuffer, 0, sizeof(ppgBuffer));
    memset(intervalBuffer, 0, sizeof(intervalBuffer));
}

void PPG::begin() {
    _exit = false;
    running = true;
    currentState = MEASURING;
    menuSelection = 0;
    currentMode = IR;
    controls.mutexLed([&]() {
        controls.strip.setPixelColor(0, 0, 0, 0);
        controls.strip.show();
    });

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
            return;
        }
    } else {
        vTaskResume(taskHandle);
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
    if (now - lastPPGReadTime < ppgInterval) {
        return;
    }
    lastPPGReadTime = now;

    const bool fingerOn = controls.fingerTouched();
    bool peakDetected = false;

    if (fingerOn) {
        const int rawValue = readPPG();

        emaValue = filterWeight * rawValue + (1.0f - filterWeight) * emaValue;
        currentValue = static_cast<int>(round(emaValue));

        if (!displayBaselineLocked) {
            displayBaselineSum += currentValue;
            displayBaselineCounter++;

            if (displayBaselineCounter >= displayBaselineSampleCount) {
                displayBaseline = static_cast<int>(displayBaselineSum / displayBaselineCounter);
                displayBaselineLocked = true;
            }
        }

        ppgBuffer[bufferIndex] = currentValue;
        bufferIndex = (bufferIndex + 1) % bufferSize;

        adjThreshold = calculateDynamicThreshold();
        peakDetected = detectPeak(currentValue, adjThreshold, now);
        if (peakDetected) {
            blinkPeak(true);
        }

    } else {
        signalAboveThreshold = false;
        lastPeakMillis = 0;
        currentValue = 0;
        emaValue = 0.0f;

        lockData();
        bpmIsValid = false;
        unlockData();
    }

    drawMeasurementScreen(fingerOn);
}

void PPG::drawMeasurementScreen(bool fingerOn) {
    controls.mutexDisplay([&]() {
        auto& display = controls.display;
        display.clearDisplay();

        if (fingerOn) {
            drawPPGCurve(display);
        } else {
            display.setTextSize(1);
            display.setTextColor(SH110X_WHITE);

            display.setCursor((128 - 13 * 6) / 2, 28);
            display.println("Prilozte prst");
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
        if (lastPeakMillis != 0 && interval >= beatInterval) {
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
    intervalIndex = (intervalIndex + 1) % intervalBufferSize;

    unsigned long sum = 0;
    int count = 0;
    for (int i = 0; i < intervalBufferSize; ++i) {
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
    if (bpm < bpmValidMin || bpm > bpmValidMax) {
        return false;
    }

    if (!bpmIsValid) {
        return true;
    }

    return abs(bpm - lastAcceptedBPM) <= bpmDiff;
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

            display.setCursor((DISPLAY_WIDTH - 8 * 3) / 2, 0);
            display.println("PPG");

            display.setCursor((DISPLAY_WIDTH - 9 * 6) / 2, 20);
            display.println("Kalibrace");

            display.setCursor((DISPLAY_WIDTH - 11 * 6) / 2, 32);
            display.println("Fotosenzoru");

            display.drawRect(13, 46, 102, 12, SH110X_WHITE);
            display.fillRect(14, 47, progressWidth, 10, SH110X_WHITE);

            display.display();
        });

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    resetMeasurementState();
    ambientBaseline = sum / totalSamples;
    if (currentMode != RED) {
        currentMode = IR;
    }

    controls.mutexDisplay([&]() {
        auto& display = controls.display;
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);

        display.setCursor((DISPLAY_WIDTH - 8 * 3) / 2, 0);
        display.println("PPG");

        display.setCursor((DISPLAY_WIDTH - 9 * 6) / 2, 24);
        display.println("Kalibrace");

        display.setCursor((DISPLAY_WIDTH - 10 * 6) / 2, 36);
        display.println("dokoncena");

        display.display();
    });

    vTaskDelay(pdMS_TO_TICKS(700));
}

int PPG::readPPG() {
    return analogRead(controls.PPG_PIN) - ambientBaseline;

}

void PPG::drawPPGCurve(Adafruit_SH1106G& display) {
    constexpr int topMargin = 10;
    constexpr int bottomMargin = 2;

    const int drawTop = topMargin;
    const int drawBottom = DISPLAY_HEIGHT - bottomMargin - 1;
    const int drawHeight = drawBottom - drawTop;

    constexpr int manualAmplitude = 1000;
    const int windowSize = min(bufferSize, 60);

    if (windowSize < 2 || drawHeight <= 0) {
        return;
    }

    const int baseIdx = (bufferIndex - windowSize + bufferSize) % bufferSize;

    int minVal = ppgBuffer[baseIdx];
    int maxVal = ppgBuffer[baseIdx];
    long sum = 0;

    for (int i = 0; i < windowSize; ++i) {
        const int idx = (baseIdx + i) % bufferSize;
        const int val = ppgBuffer[idx];

        if (val < minVal) minVal = val;
        if (val > maxVal) maxVal = val;
        sum += val;
    }

    float displayMin = 0.0f;
    float displayMax = 0.0f;

    if (autoZoomEnabled) {
        displayMin = static_cast<float>(minVal);
        displayMax = static_cast<float>(maxVal);

        const float padding = (displayMax - displayMin) * 0.08f;
        displayMin -= padding;
        displayMax += padding;
    } else {
        displayMin = displayBaseline - manualAmplitude;
        displayMax = displayBaseline + manualAmplitude;
    }

    if (fabsf(displayMax - displayMin) < 1.0f) {
        displayMax = displayMin + 1.0f;
    }

    const float xStep = static_cast<float>(DISPLAY_WIDTH - 1) / static_cast<float>(windowSize - 1);

    for (int i = 1; i < windowSize; ++i) {
        const int prevIndex = (baseIdx + i - 1) % bufferSize;
        const int currIndex = (baseIdx + i) % bufferSize;

        const float v1 = static_cast<float>(ppgBuffer[prevIndex]);
        const float v2 = static_cast<float>(ppgBuffer[currIndex]);

        const float norm1 = (v1 - displayMin) / (displayMax - displayMin);
        const float norm2 = (v2 - displayMin) / (displayMax - displayMin);

        const int x1 = static_cast<int>(round((i - 1) * xStep));
        const int x2 = static_cast<int>(round(i * xStep));

        const int y1 = constrain(
            static_cast<int>(round(drawBottom - norm1 * drawHeight)),
            drawTop,
            drawBottom
        );

        const int y2 = constrain(
            static_cast<int>(round(drawBottom - norm2 * drawHeight)),
            drawTop,
            drawBottom
        );

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

    int maxVal = ppgBuffer[(bufferIndex - 1 + bufferSize) % bufferSize];
    int minVal = maxVal;

    for (int i = 0; i < window; ++i) {
        const int idx = (bufferIndex - 1 - i + bufferSize) % bufferSize;
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
        display.setCursor((DISPLAY_WIDTH - 8 * 10) / 2, 0);
        display.println("NASTAVENI");

        for (int i = 0; i < menuItemCount; ++i) {
            const int y = 20 + i * 12;

            display.setCursor(10, y);
            display.print((i == menuSelection) ? "> " : "  ");
            display.print(menuItems[i]);

            if (i == MENU_AUTOZOOM) {
                display.setCursor(92, y);
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

    if (ledOn && now - lastPeakLedTime >= bpmBlinkTime) {
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

void PPG::registerWebRoutes() {
    settings.server().on("/ppg/update", HTTP_GET, [this](AsyncWebServerRequest* request) {
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

    settings.server().on("/ppg/toggle-led", HTTP_GET, [this](AsyncWebServerRequest* request) {
        currentMode = (currentMode == IR) ? RED : IR;
        request->send(200, "text/plain", "ok");
    });

    settings.server().on("/ppg/autozoom", HTTP_GET, [this](AsyncWebServerRequest* request) {
        autoZoomEnabled = !autoZoomEnabled;
        request->send(200, "text/plain", "ok");
    });

    settings.server().on("/ppg/calibrate", HTTP_GET, [this](AsyncWebServerRequest* request) {
        currentState = CALIBRATION;
        request->send(200, "text/plain", "ok");
    });

    settings.server().on("/ppg/exit", HTTP_GET, [this](AsyncWebServerRequest* request) {
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
