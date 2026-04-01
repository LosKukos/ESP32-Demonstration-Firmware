#include <RGBMENU.h>

namespace {
constexpr uint32_t RGB_TASK_STACK_WORDS = 3072;
StaticTask_t rgbTaskBuffer;
StackType_t rgbTaskStack[RGB_TASK_STACK_WORDS];
}

RGBMenu::RGBMenu(Controls& ctrl, Settings& settingsRef)
    : controls(ctrl), settings(settingsRef) {
    dataMutex = xSemaphoreCreateMutex();
}

void RGBMenu::start() {
    resetState(false);
    running = true;
    applyLedColor(0, 0, 0);

    if (taskHandle == nullptr) {
        taskHandle = xTaskCreateStaticPinnedToCore(
            task,
            "RGBMenuTask",
            RGB_TASK_STACK_WORDS,
            this,
            1,
            rgbTaskStack,
            &rgbTaskBuffer,
            1
        );

        if (taskHandle == nullptr) {
            running = false;
            Serial.println("[RGB] static task create FAIL");
            return;
        }

        Serial.println("[RGB] static task create OK");
    } else {
        vTaskResume(taskHandle);
        Serial.println("[RGB] task resumed");
    }
}

bool RGBMenu::shouldExit() {
    bool exitRequested = false;

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        exitRequested = state.exitRequested;
        xSemaphoreGive(dataMutex);
    }

    return exitRequested;
}

bool RGBMenu::isRunning() {
    return running;
}

void RGBMenu::requestStop() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        state.exitRequested = true;
        state.dirty = true;
        xSemaphoreGive(dataMutex);
    }
}

void RGBMenu::resetState(bool requestExit) {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        state.rgb[0] = 0;
        state.rgb[1] = 0;
        state.rgb[2] = 0;
        state.currentItem = 0;
        state.adjustingValue = false;
        state.exitRequested = requestExit;
        state.dirty = true;
        xSemaphoreGive(dataMutex);
    }
}

RGBMenu::State RGBMenu::copyState() {
    State snapshot{};

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        snapshot = state;
        xSemaphoreGive(dataMutex);
    }

    return snapshot;
}

void RGBMenu::handleTouch() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return;
    }

    if (state.currentItem < 3) {
        state.adjustingValue = !state.adjustingValue;
    } else if (state.currentItem == 3) {
        state.rgb[0] = 0;
        state.rgb[1] = 0;
        state.rgb[2] = 0;
        state.adjustingValue = false;
    } else {
        state.rgb[0] = 0;
        state.rgb[1] = 0;
        state.rgb[2] = 0;
        state.adjustingValue = false;
        state.exitRequested = true;
    }

    state.dirty = true;
    xSemaphoreGive(dataMutex);
}

void RGBMenu::applyLedColor(uint8_t r, uint8_t g, uint8_t b) {
    controls.mutexLed([&]() {
        controls.strip.setPixelColor(0, r, g, b);
        controls.strip.show();
    });
}

void RGBMenu::task(void* pvParameters) {
    RGBMenu* self = static_cast<RGBMenu*>(pvParameters);
    uint8_t lastApplied[3] = {255, 255, 255};

    for (;;) {
        if (!self->running) {
            vTaskSuspend(nullptr);
        }

        if (self->controls.FingerTouchedFlag()) {
            self->handleTouch();
        }

        if (xSemaphoreTake(self->dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (!self->state.adjustingValue) {
                if (self->controls.leftPressed) {
                    self->controls.leftPressed = false;
                    self->state.currentItem = (self->state.currentItem + self->menuLength - 1) % self->menuLength;
                    self->state.dirty = true;
                }

                if (self->controls.rightPressed) {
                    self->controls.rightPressed = false;
                    self->state.currentItem = (self->state.currentItem + 1) % self->menuLength;
                    self->state.dirty = true;
                }
            } else {
                if (self->controls.leftPressed) {
                    self->controls.leftPressed = false;
                    uint16_t value = self->state.rgb[self->state.currentItem] + 10;
                    self->state.rgb[self->state.currentItem] = static_cast<uint8_t>(value > 255 ? 255 : value);
                    self->state.dirty = true;
                }

                if (self->controls.rightPressed) {
                    self->controls.rightPressed = false;
                    int value = static_cast<int>(self->state.rgb[self->state.currentItem]) - 10;
                    self->state.rgb[self->state.currentItem] = static_cast<uint8_t>(value < 0 ? 0 : value);
                    self->state.dirty = true;
                }
            }

            bool exitRequested = self->state.exitRequested;
            bool dirty = self->state.dirty;
            State snapshot = self->state;
            self->state.dirty = false;
            xSemaphoreGive(self->dataMutex);

            if (dirty) {
                if (snapshot.rgb[0] != lastApplied[0] ||
                    snapshot.rgb[1] != lastApplied[1] ||
                    snapshot.rgb[2] != lastApplied[2]) {
                    lastApplied[0] = snapshot.rgb[0];
                    lastApplied[1] = snapshot.rgb[1];
                    lastApplied[2] = snapshot.rgb[2];
                    self->applyLedColor(snapshot.rgb[0], snapshot.rgb[1], snapshot.rgb[2]);
                }

                self->drawMenu(snapshot);
            }

            if (exitRequested) {
                self->applyLedColor(0, 0, 0);
                lastApplied[0] = 0;
                lastApplied[1] = 0;
                lastApplied[2] = 0;
                self->running = false;

                UBaseType_t highWaterWords = uxTaskGetStackHighWaterMark(nullptr);
                Serial.printf("[RGB] task suspended | stack free minimum: %u bytes\n",
                              (unsigned int)(highWaterWords * sizeof(StackType_t)));

                vTaskSuspend(nullptr);
                continue;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

void RGBMenu::drawSlider(int x, int y, int w, int h, int value, const char* label, bool selected) {
    controls.display.drawRect(x, y, w, h, SH110X_WHITE);
    controls.display.fillRect(x, y, map(value, 0, 255, 0, w), h, SH110X_WHITE);

    controls.display.setCursor(x + w + 4, y);
    controls.display.print(label);
    controls.display.print(" ");
    controls.display.print(value);

    if (selected) {
        controls.display.setCursor(x - 6, y);
        controls.display.print(">");
    }
}

void RGBMenu::drawMenu(const State& snapshot) {
    controls.mutexDisplay([&]() {
        controls.display.clearDisplay();

        const int yOffset = 2;
        const int spacing = 10;

        for (int i = 0; i < menuLength; i++) {
            if (i < 3) {
                drawSlider(5, yOffset + i * spacing, 80, 8, snapshot.rgb[i], menuItems[i], i == snapshot.currentItem);
            } else {
                controls.display.setCursor(5, yOffset + i * spacing + 10);
                if (i == snapshot.currentItem) {
                    controls.display.print(">");
                }
                controls.display.print(menuItems[i]);
            }
        }

        controls.display.setCursor(5, yOffset + 3 * spacing);
        char hexColor[7];
        snprintf(hexColor, sizeof(hexColor), "%02X%02X%02X", snapshot.rgb[0], snapshot.rgb[1], snapshot.rgb[2]);
        controls.display.print("HEX: #");
        controls.display.print(hexColor);

        controls.display.display();
    });
}

void RGBMenu::web_RGBMenu() {
    if (webRoutesRegistered) {
        return;
    }
    webRoutesRegistered = true;

    settings.server().on("/rgb/update", HTTP_GET, [this](AsyncWebServerRequest* request) {
        bool updated = false;

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (!state.exitRequested) {
                if (request->hasParam("r")) {
                    state.rgb[0] = static_cast<uint8_t>(constrain(request->getParam("r")->value().toInt(), 0, 255));
                    updated = true;
                }

                if (request->hasParam("g")) {
                    state.rgb[1] = static_cast<uint8_t>(constrain(request->getParam("g")->value().toInt(), 0, 255));
                    updated = true;
                }

                if (request->hasParam("b")) {
                    state.rgb[2] = static_cast<uint8_t>(constrain(request->getParam("b")->value().toInt(), 0, 255));
                    updated = true;
                }

                if (updated) {
                    state.dirty = true;
                }
            }
            xSemaphoreGive(dataMutex);
        }

        request->send(200, "text/plain", updated ? "ok" : "nochange");
    });

    settings.server().on("/rgb/reset", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            state.rgb[0] = 0;
            state.rgb[1] = 0;
            state.rgb[2] = 0;
            state.adjustingValue = false;
            state.dirty = true;
            xSemaphoreGive(dataMutex);
        }

        request->send(200, "text/plain", "ok");
    });

    settings.server().on("/rgb/exit", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            state.rgb[0] = 0;
            state.rgb[1] = 0;
            state.rgb[2] = 0;
            state.adjustingValue = false;
            state.exitRequested = true;
            state.dirty = true;
            xSemaphoreGive(dataMutex);
        }

        request->send(200, "text/plain", "ok");
    });

    settings.server().on("/rgb/state", HTTP_GET, [this](AsyncWebServerRequest* request) {
        State snapshot = copyState();

        char response[128];
        snprintf(
            response,
            sizeof(response),
            "{\"r\":%u,\"g\":%u,\"b\":%u,\"currentItem\":%u,\"adjusting\":%s,\"exit\":%s}",
            snapshot.rgb[0],
            snapshot.rgb[1],
            snapshot.rgb[2],
            snapshot.currentItem,
            snapshot.adjustingValue ? "true" : "false",
            snapshot.exitRequested ? "true" : "false"
        );

        request->send(200, "application/json", response);
    });
}