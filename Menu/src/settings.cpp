#include "settings.h"

BluetoothSerial SerialBT;

namespace {
    // Statická paměť pro FreeRTOS task modulu
    constexpr uint32_t SETTINGS_TASK_STACK_WORDS = 2048;
    StaticTask_t settingsTaskBuffer;
    StackType_t settingsTaskStack[SETTINGS_TASK_STACK_WORDS];
}

Settings::Settings(Controls& controlsRef)
    : controls(controlsRef), _server(80) {}

AsyncWebServer& Settings::server() {
    return _server;
}

void Settings::webStart() {
    // Spuštění webserveru pouze pokud ještě neběží
    if (!webServerStarted) {
        _server.begin();
        webServerStarted = true;
    }
}

void Settings::wifiStartup() {
    // Spuštění WiFi přístupového bodu
    WiFi.mode(WIFI_AP);
    WiFi.softAP(wifi_ssid_runtime.length() ? wifi_ssid_runtime.c_str() : WIFI_default);
    WiFi.setSleep(false);
}

void Settings::startBluetooth() {
    if (currentConnectivityMode == ConnectivityMode::BT) {
        return;
    }

    // Před spuštěním Bluetooth se vypne WiFi a webserver
    stopWifiWeb();
    delay(300);

    const char* btName = bt_name_runtime.length() ? bt_name_runtime.c_str() : BT_default;
    SerialBT.begin(btName);

    currentConnectivityMode = ConnectivityMode::BT;
}

void Settings::stopBluetooth() {
    // Vypnutí Bluetooth komunikace
    SerialBT.end();
    delay(200);

    currentConnectivityMode = ConnectivityMode::WIFI;
}

void Settings::startWifiWeb() {
    if (currentConnectivityMode == ConnectivityMode::WIFI) {
        return;
    }

    // Před spuštěním WiFi se vypne Bluetooth
    stopBluetooth();
    delay(300);

    wifiStartup();
    delay(200);

    webStart();

    currentConnectivityMode = ConnectivityMode::WIFI;
}

void Settings::stopWifiWeb() {
    // Vypnutí webserveru a WiFi přístupového bodu
    if (webServerStarted) {
        _server.end();
        webServerStarted = false;
    }

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(200);

    currentConnectivityMode = ConnectivityMode::BT;
}

void Settings::switchConnectivity(ConnectivityMode newMode) {
    if (newMode == currentConnectivityMode) {
        return;
    }

    // Přepnutí mezi Bluetooth a WiFi režimem
    switch (newMode) {
        case ConnectivityMode::BT:
            startBluetooth();
            break;

        case ConnectivityMode::WIFI:
            startWifiWeb();
            break;
    }
}

void Settings::init() {
    // Výchozí nastavení komunikačního režimu
    currentConnectivityMode = ConnectivityMode::BT;
    webServerStarted = false;
}

void Settings::begin() {
    // Reset stavu modulu
    _exit = false;
    running = true;
    state = MENU;
    selected = 0;
    firstVisibleItem = 0;

    // Vytvoření nebo probuzení tasku modulu
    if (taskHandle == nullptr) {
        taskHandle = xTaskCreateStaticPinnedToCore(
            task,
            "SettingsTask",
            SETTINGS_TASK_STACK_WORDS,
            this,
            1,
            settingsTaskStack,
            &settingsTaskBuffer,
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

String Settings::readSerialLine() {
    // Čtení vstupu ze sériové linky nebo Bluetooth
    if (Serial.available()) return Serial.readStringUntil('\n');
    if (SerialBT.available()) return SerialBT.readStringUntil('\n');
    return "";
}

void Settings::drawMenu() {
    controls.mutexDisplay([&]() {
        controls.display.clearDisplay();
        controls.display.setTextSize(1);
        controls.display.setTextColor(SH110X_WHITE);

        controls.display.setCursor((SCREEN_WIDTH - 8 * 10) / 2, 0);
        controls.display.println("NASTAVENI");

        for (int i = 0; i < visibleItems; i++) {
            const int itemIndex = firstVisibleItem + i;
            if (itemIndex >= menuLength) {
                break;
            }

            controls.display.setCursor(10, menuTopOffset + i * menuItemHeight);
            controls.display.print(itemIndex == selected ? "> " : "  ");
            controls.display.println(menuItems[itemIndex]);
        }

        controls.display.display();
    });
}

void Settings::updateMenuSelection(int direction) {
    // Posun výběru v menu
    selected = (selected + direction + menuLength) % menuLength;

    if (selected < firstVisibleItem) {
        firstVisibleItem = selected;
    }

    if (selected >= firstVisibleItem + visibleItems) {
        firstVisibleItem = selected - visibleItems + 1;
    }

    if (selected == 0) {
        firstVisibleItem = 0;
    }

    if (selected == menuLength - 1) {
        firstVisibleItem = max(menuLength - visibleItems, 0);
    }
}

void Settings::task(void *pvParameters) {
    Settings* self = static_cast<Settings*>(pvParameters);

    for (;;) {
        // Pokud dojde k ukončení, task se uspí
        if (!self->running) {
            vTaskSuspend(nullptr);
        }

        // Ukončení modulu a návrat komunikace do WiFi režimu
        if (self->_exit) {
            self->state = MENU;
            self->selected = 0;
            self->firstVisibleItem = 0;
            self->running = false;
            self->switchConnectivity(ConnectivityMode::WIFI);
            vTaskSuspend(nullptr);
            continue;
        }

        // Stavový automat modulu
        switch (self->state) {
            case MENU:
                self->drawMenu();
                self->updateRainbow();

                if (self->controls.leftPressed) {
                    self->updateMenuSelection(-1);
                    self->controls.leftPressed = false;
                }

                if (self->controls.rightPressed) {
                    self->updateMenuSelection(1);
                    self->controls.rightPressed = false;
                }

                if (self->controls.FingerTouchedFlag()) {
                    if (self->selected == 0) {
                        self->switchConnectivity(ConnectivityMode::WIFI);

                        self->controls.mutexLed([&]() {
                            self->controls.strip.setPixelColor(0, 255, 255, 255);
                            self->controls.strip.show();
                        });

                        self->state = ABOUT;
                    }

                    if (self->selected == 1) {
                        self->switchConnectivity(ConnectivityMode::BT);

                        self->controls.mutexLed([&]() {
                            self->controls.strip.setPixelColor(0, 0, 0, 255);
                            self->controls.strip.show();
                        });

                        Serial.println("Zadejte nové jméno BT");
                        if (SerialBT.hasClient()) {
                            SerialBT.println("Zadejte nové jméno BT");
                        }

                        self->state = BT_READ;
                    }

                    if (self->selected == 2) {
                        self->switchConnectivity(ConnectivityMode::BT);

                        self->controls.mutexLed([&]() {
                            self->controls.strip.setPixelColor(0, 0, 255, 0);
                            self->controls.strip.show();
                        });

                        Serial.println("Zadejte nové SSID");
                        if (SerialBT.hasClient()) {
                            SerialBT.println("Zadejte nové SSID");
                        }

                        self->state = WIFI_SSID_READ;
                    }

                    if (self->selected == 3) {
                        self->switchConnectivity(ConnectivityMode::BT);

                        self->controls.mutexLed([&]() {
                            self->controls.strip.setPixelColor(0, 255, 0, 0);
                            self->controls.strip.show();
                        });

                        self->state = DEBUG;
                    }

                    if (self->selected == 4) {
                        self->_exit = true;
                    }
                }
                break;

            case BT_READ:
                self->drawBTRead();
                break;

            case BT_CONFIRM:
                self->drawBTConfirm();
                break;

            case WIFI_SSID_READ:
                self->drawWiFiSSIDRead();
                break;

            case WIFI_CONFIRM:
                self->drawWiFiConfirm();
                break;

            case ABOUT:
                self->drawAbout();
                break;
            
            case DEBUG:
                self->drawDebug();
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void Settings::drawBTRead() {
    if (controls.leftPressed) {
        controls.leftPressed = false;
        state = MENU;
        drawMenu();
        return;
    }

    controls.mutexDisplay([&]() {
        controls.display.clearDisplay();
        controls.display.setTextSize(2);
        controls.display.setTextColor(SH110X_WHITE);
        controls.display.setCursor(0, 0);
        controls.display.println("Bluetooth");

        controls.display.setTextSize(1);
        controls.display.setCursor(10, 24);
        controls.display.println("Zadej nove jmeno");

        controls.display.setCursor(10, 56);
        controls.display.println("D16 ZPET");

        controls.display.display();
    });

    // Uložení nového Bluetooth názvu
    String input = readSerialLine();
    if (input.length()) {
        bt_name_runtime = input;
        state = BT_CONFIRM;
    }
}

void Settings::drawBTConfirm() {
    if (controls.leftPressed) {
        controls.leftPressed = false;
        state = MENU;
        drawMenu();
        return;
    }

    if (controls.rightPressed) {
        controls.rightPressed = false;

        // Potvrzení a spuštění Bluetooth s novým názvem
        switchConnectivity(ConnectivityMode::BT);

        state = MENU;
        drawMenu();
        return;
    }

    controls.mutexDisplay([&]() {
        controls.display.clearDisplay();
        controls.display.setTextSize(2);
        controls.display.setTextColor(SH110X_WHITE);
        controls.display.setCursor(0, 0);
        controls.display.println("Bluetooth");

        controls.display.setTextSize(1);
        controls.display.setCursor(10, 24);
        controls.display.println("Novy nazev:");

        controls.display.setCursor(10, 36);
        controls.display.println(bt_name_runtime);

        controls.display.setCursor(10, 56);
        controls.display.println("D16 ZPET D17 OK");

        controls.display.display();
    });
}

void Settings::drawWiFiSSIDRead() {
    if (controls.leftPressed) {
        controls.leftPressed = false;
        state = MENU;
        drawMenu();
        return;
    }

    controls.mutexDisplay([&]() {
        controls.display.clearDisplay();
        controls.display.setTextSize(2);
        controls.display.setTextColor(SH110X_WHITE);
        controls.display.setCursor(0, 0);
        controls.display.println("WiFi");

        controls.display.setTextSize(1);
        controls.display.setCursor(10, 24);
        controls.display.println("Zadej SSID");

        controls.display.setCursor(10, 56);
        controls.display.println("D16 ZPET");

        controls.display.display();
    });

    // Uložení nového názvu WiFi sítě
    String input = readSerialLine();
    input.trim();

    if (input.length()) {
        wifi_ssid_runtime = input;
        state = WIFI_CONFIRM;
    }
}

void Settings::drawWiFiConfirm() {
    if (controls.leftPressed) {
        controls.leftPressed = false;
        state = MENU;
        drawMenu();
        return;
    }

    if (controls.rightPressed) {
        controls.rightPressed = false;

        // Potvrzení a spuštění WiFi s novým SSID
        switchConnectivity(ConnectivityMode::WIFI);

        controls.mutexDisplay([&]() {
            controls.display.clearDisplay();
            controls.display.setTextSize(2);
            controls.display.setTextColor(SH110X_WHITE);
            controls.display.setCursor(0, 0);
            controls.display.println("WiFi");

            controls.display.setTextSize(1);
            controls.display.setCursor(10, 24);
            controls.display.println("AP spusten");

            controls.display.setCursor(10, 36);
            controls.display.print("IP: ");
            controls.display.println(WiFi.softAPIP());

            controls.display.display();
        });

        delay(1500);

        state = MENU;
        drawMenu();
        return;
    }

    controls.mutexDisplay([&]() {
        controls.display.clearDisplay();
        controls.display.setTextSize(2);
        controls.display.setTextColor(SH110X_WHITE);
        controls.display.setCursor(0, 0);
        controls.display.println("WiFi");

        controls.display.setTextSize(1);
        controls.display.setCursor(10, 24);
        controls.display.println("SSID:");

        controls.display.setCursor(10, 36);
        controls.display.println(wifi_ssid_runtime);

        controls.display.setCursor(10, 56);
        controls.display.println("D16 ZPET D17 OK");

        controls.display.display();
    });
}

uint32_t Settings::wheelColor(byte wheelPos) {
    // Přepočet pozice na barvu pro animaci LED
    wheelPos = 255 - wheelPos;

    if (wheelPos < 85) {
        return controls.strip.Color(255 - wheelPos * 3, 0, wheelPos * 3);
    }

    if (wheelPos < 170) {
        wheelPos -= 85;
        return controls.strip.Color(0, wheelPos * 3, 255 - wheelPos * 3);
    }

    wheelPos -= 170;
    return controls.strip.Color(wheelPos * 3, 255 - wheelPos * 3, 0);
}

bool Settings::shouldExit() {
    return _exit;
}

void Settings::drawAbout() {
    if (controls.leftPressed || controls.rightPressed || controls.FingerTouchedFlag()) {
        controls.leftPressed = false;
        controls.rightPressed = false;
        state = MENU;
        drawMenu();
        return;
    }

    controls.mutexDisplay([&]() {
        controls.display.clearDisplay();
        controls.display.setTextSize(1);
        controls.display.setTextColor(SH110X_WHITE);

        controls.display.setCursor((SCREEN_WIDTH - 8 * 4) / 2, 0);
        controls.display.println("INFO");

        controls.display.setCursor(10, 14);
        controls.display.print("SSID: ");
        if (wifi_ssid_runtime == "" || wifi_ssid_runtime.length() == 0) {
            controls.display.println(WiFi.softAPSSID());
        } else {
            controls.display.println(wifi_ssid_runtime);
        }

        controls.display.setCursor(10, 26);
        controls.display.print("IP: ");
        controls.display.println(WiFi.softAPIP());

        controls.display.setCursor(10, 38);
        controls.display.print("BT: ");
        if (bt_name_runtime == "" || bt_name_runtime.length() == 0) {
            controls.display.println(BT_default);
        } else {
            controls.display.println(bt_name_runtime);
        }

        controls.display.display();
    });
}

void Settings::drawDebug() {
    if (controls.leftPressed || controls.rightPressed || controls.FingerTouchedFlag()) {
        controls.leftPressed = false;
        controls.rightPressed = false;
        state = MENU;
        drawMenu();
        return;
    }

    controls.mutexDisplay([&]() {
        controls.display.clearDisplay();
        controls.display.setTextSize(1);
        controls.display.setTextColor(SH110X_WHITE);

        controls.display.setCursor((SCREEN_WIDTH - 8 * 5) / 2, 0);
        controls.display.println("DEBUG");

        controls.display.setCursor(10, 18);
        controls.display.println("Zadejte prikaz:");

        controls.display.display();
    });

    // Čtení příkazu ze sériové linky nebo Bluetooth
    String input = readSerialLine();
    input.trim();

    if (input.length() == 0) {
        return;
    }

    Serial.println("> " + input);
    if (SerialBT.hasClient()) {
        SerialBT.println("> " + input);
    }

    // Výpis dostupných debug příkazů
    if (input.equalsIgnoreCase("help")) {
        Serial.println("Prikazy: help, heap, fs, flash, runtime, touch, reset, exit");
        if (SerialBT.hasClient()) {
            SerialBT.println("Prikazy: help, heap, fs, flash, runtime, touch, reset, exit");
        }
    }

    // Informace o využití paměti
    else if (input.equalsIgnoreCase("heap")) {
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t minHeap = ESP.getMinFreeHeap();
        uint32_t maxAlloc = ESP.getMaxAllocHeap();

        Serial.println("Memory");
        Serial.println("Free: " + String(freeHeap) + " B");
        Serial.println("Peak usage: " + String(minHeap) + " B min free");
        Serial.println("Fragmentation: max alloc " + String(maxAlloc) + " B (bigger = better)");

        if (SerialBT.hasClient()) {
            SerialBT.println("Memory");
            SerialBT.println("Free: " + String(freeHeap) + " B");
            SerialBT.println("Peak usage: " + String(minHeap) + " B min free");
            SerialBT.println("Fragmentation: max alloc " + String(maxAlloc) + " B (bigger = better)");
        }
    }

    // Informace o souborovém systému
    else if (input.equalsIgnoreCase("fs")) {
        uint32_t total = LittleFS.totalBytes();
        uint32_t used = LittleFS.usedBytes();
        uint32_t free = total - used;

        uint8_t usagePercent = (total > 0) ? (used * 100 / total) : 0;

        Serial.println("Storage");
        Serial.println("Used: " + String(used) + " / " + String(total) + " B (" + String(usagePercent) + " %)");
        Serial.println("Free: " + String(free) + " B");

        if (SerialBT.hasClient()) {
            SerialBT.println("Storage");
            SerialBT.println("Used: " + String(used) + " / " + String(total) + " B (" + String(usagePercent) + " %)");
            SerialBT.println("Free: " + String(free) + " B");
        }
    }

    // Informace o velikosti firmware a flash paměti
    else if (input.equalsIgnoreCase("flash")) {
        uint32_t flashSize = ESP.getFlashChipSize();
        uint32_t sketchSize = ESP.getSketchSize();
        uint32_t freeSketch = ESP.getFreeSketchSpace();

        uint32_t flashMB = flashSize / (1024 * 1024);
        uint32_t sketchKB = sketchSize / 1024;
        uint32_t freeKB = freeSketch / 1024;

        uint32_t usedPercent = (flashSize > 0) ? (sketchSize * 100 / flashSize) : 0;

        Serial.println("Flash");
        Serial.println("Total: " + String(flashMB) + " MB");
        Serial.println("Firmware: " + String(sketchKB) + " kB (" + String(usedPercent) + " %)");
        Serial.println("Free space: " + String(freeKB) + " kB");

        if (SerialBT.hasClient()) {
            SerialBT.println("Flash");
            SerialBT.println("Total: " + String(flashMB) + " MB");
            SerialBT.println("Firmware: " + String(sketchKB) + " kB (" + String(usedPercent) + " %)");
            SerialBT.println("Free space: " + String(freeKB) + " kB");
        }
    }

    // Výpis doby běhu zařízení
    else if (input.equalsIgnoreCase("runtime")) {
        uint32_t totalSeconds = millis() / 1000;

        uint32_t hours = totalSeconds / 3600;
        uint32_t minutes = (totalSeconds % 3600) / 60;
        uint32_t seconds = totalSeconds % 60;

        char buffer[20];
        snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu", hours, minutes, seconds);

        String line = "Uptime: " + String(buffer);

        Serial.println(line);
        if (SerialBT.hasClient()) {
            SerialBT.println(line);
        }
    }

    // Rekalibrace kapacitního senzoru
    else if (input.equalsIgnoreCase("touch")) {
        controls.calibrateTouch();
        controls.resetTouchState();

        Serial.println("Touch recalibrated");
        if (SerialBT.hasClient()) {
            SerialBT.println("Touch recalibrated");
        }
    }

    // Restart zařízení
    else if (input.equalsIgnoreCase("reset")) {
        Serial.println("Restarting...");
        if (SerialBT.hasClient()) {
            SerialBT.println("
