#include <RGBMENU.h>

RGBMenu::RGBMenu(Controls& ctrl)
    : controls(ctrl), currentItem(0), adjustingValue(false) {
}

void RGBMenu::start() {

    if (taskHandle != NULL) return;

    currentItem = 0;
    _exit = false;
    adjustingValue = false;
    needsUpdate = true;

    sliderValues[0] = 0;
    sliderValues[1] = 0;
    sliderValues[2] = 0;

    pendingValues[0] = 0;
    pendingValues[1] = 0;
    pendingValues[2] = 0;

    pendingUpdate = false;

    controls.mutexLed([&]() {
    controls.strip.setPixelColor(0, 0, 0, 0);
    controls.strip.show();
    });

    dataMutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(
        task,
        "RGBMenuTask",
        4096,
        this,
        1,
        &taskHandle,
        1
    );
}

void RGBMenu::handleTouch() {

    if (currentItem < 3) {
        adjustingValue = !adjustingValue;
        return;
    }

    if (currentItem == 3) { // Restart
        sliderValues[0] = sliderValues[1] = sliderValues[2] = 0;
        adjustingValue = false;
        needsUpdate = true;
        return;
    }

    if (currentItem == 4) { // Exit
        sliderValues[0] = sliderValues[1] = sliderValues[2] = 0;
        adjustingValue = false;
        _exit = true;
        needsUpdate = true;
        return;
    }
}

void RGBMenu::task(void *pvParameters) {
    RGBMenu* self = static_cast<RGBMenu*>(pvParameters);

    static uint8_t lastR = 255, lastG = 255, lastB = 255;

    for (;;) {

        if (self->_exit) {
            break;
        }

        bool changed = false;

        // ===== APPLY WEB VALUES =====
        if (xSemaphoreTake(self->dataMutex, pdMS_TO_TICKS(5))) {

            if (self->pendingUpdate) {

                if (self->sliderValues[0] != self->pendingValues[0] ||
                    self->sliderValues[1] != self->pendingValues[1] ||
                    self->sliderValues[2] != self->pendingValues[2]) {

                    self->sliderValues[0] = self->pendingValues[0];
                    self->sliderValues[1] = self->pendingValues[1];
                    self->sliderValues[2] = self->pendingValues[2];

                    self->needsUpdate = true;
                    changed = true;
                }

                self->pendingUpdate = false;
            }

            xSemaphoreGive(self->dataMutex);
        }

        // ===== TOUCH =====
        if (self->controls.FingerTouchedFlag()) {
            self->handleTouch();
            changed = true;
        }

        // ===== NAVIGATION =====
        if (!self->adjustingValue) {

            if (self->controls.leftPressed) {
                self->controls.leftPressed = false;
                self->currentItem = (self->currentItem - 1 + self->menuLength) % self->menuLength;
                changed = true;
            }

            if (self->controls.rightPressed) {
                self->controls.rightPressed = false;
                self->currentItem = (self->currentItem + 1) % self->menuLength;
                changed = true;
            }

        } else {

            if (self->controls.leftPressed) {
                self->controls.leftPressed = false;
                self->sliderValues[self->currentItem] =
                    min(self->sliderValues[self->currentItem] + 10, 255);
                changed = true;
            }

            if (self->controls.rightPressed) {
                self->controls.rightPressed = false;
                self->sliderValues[self->currentItem] =
                    max(self->sliderValues[self->currentItem] - 10, 0);
                changed = true;
            }
        }

        // ===== RENDER =====
        if (self->needsUpdate || changed) {

            self->needsUpdate = false;

            if (self->sliderValues[0] != lastR ||
                self->sliderValues[1] != lastG ||
                self->sliderValues[2] != lastB) {

                lastR = self->sliderValues[0];
                lastG = self->sliderValues[1];
                lastB = self->sliderValues[2];

                self->controls.mutexLed([&]() {
                    self->controls.strip.setPixelColor(0, lastR, lastG, lastB);
                    self->controls.strip.show();
                });
            }

            self->drawMenu();
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }
    self->taskHandle = NULL;
    vTaskDelete(NULL);
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

void RGBMenu::drawMenu() {

    controls.mutexDisplay([&]() {

        controls.display.clearDisplay();

        int yOffset = 2;
        int spacing = 10;

        for (int i = 0; i < menuLength; i++) {

            if (i < 3) {
                drawSlider(5, yOffset + i * spacing, 80, 8, sliderValues[i], menuItems[i], i == currentItem);
            } else {
                controls.display.setCursor(5, yOffset + i * spacing + 10);

                if (i == currentItem) controls.display.print(">");
                controls.display.print(menuItems[i]);
            }
        }

        // HEX
        controls.display.setCursor(5, yOffset + 3 * spacing);

        char hexColor[7];
        const char hex[] = "0123456789ABCDEF";

        hexColor[0] = hex[sliderValues[0] >> 4];
        hexColor[1] = hex[sliderValues[0] & 0x0F];
        hexColor[2] = hex[sliderValues[1] >> 4];
        hexColor[3] = hex[sliderValues[1] & 0x0F];
        hexColor[4] = hex[sliderValues[2] >> 4];
        hexColor[5] = hex[sliderValues[2] & 0x0F];
        hexColor[6] = '\0';

        controls.display.print("HEX: #");
        controls.display.print(hexColor);

        controls.display.display();
    });
}

bool RGBMenu::shouldExit() {
    return _exit;
}

bool RGBMenu::isRunning() {
    return taskHandle != NULL;
}

void RGBMenu::web_RGBMenu() {

    web.server().on("/rgb/update", HTTP_GET, [this](AsyncWebServerRequest *request) {

        if (!_exit && xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5))) {

            bool changed = false;

            if (request->hasParam("r")) {
                int val = constrain(request->getParam("r")->value().toInt(), 0, 255);
                if (val != pendingValues[0]) {
                    pendingValues[0] = val;
                    changed = true;
                }
            }

            if (request->hasParam("g")) {
                int val = constrain(request->getParam("g")->value().toInt(), 0, 255);
                if (val != pendingValues[1]) {
                    pendingValues[1] = val;
                    changed = true;
                }
            }

            if (request->hasParam("b")) {
                int val = constrain(request->getParam("b")->value().toInt(), 0, 255);
                if (val != pendingValues[2]) {
                    pendingValues[2] = val;
                    changed = true;
                }
            }

            if (changed) pendingUpdate = true;

            xSemaphoreGive(dataMutex);
        }

    request->send(200, "text/plain", "OK");
    });

    web.server().on("/rgb/reset", HTTP_GET, [this](AsyncWebServerRequest *request) {

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5))) {

            pendingValues[0] = pendingValues[1] = pendingValues[2] = 0;
            sliderValues[0] = sliderValues[1] = sliderValues[2] = 0;

            pendingUpdate = true;
            needsUpdate = true;

            xSemaphoreGive(dataMutex);
        }

        request->send(200, "text/plain", "ok");
    });

    web.server().on("/rgb/exit", HTTP_GET, [this](AsyncWebServerRequest *request) {

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5))) {

            pendingValues[0] = pendingValues[1] = pendingValues[2] = 0;
            sliderValues[0] = sliderValues[1] = sliderValues[2] = 0;

            pendingUpdate = true;

            adjustingValue = false;
            _exit = true;
            needsUpdate = true;

            xSemaphoreGive(dataMutex);
        }

        request->send(200, "text/plain", "ok");
    });
}