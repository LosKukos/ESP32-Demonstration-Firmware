#include "settings.h"

BluetoothSerial SerialBT;

namespace {
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
    if (!webServerStarted) {
        _server.begin();
        webServerStarted = true;
        Serial.println("[WEB] server started");
    }
}

void Settings::wifiStartup() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(wifi_ssid_runtime.length() ? wifi_ssid_runtime.c_str() : WIFI_default);
    WiFi.setSleep(false);

    IPAddress ip = WiFi.softAPIP();

    Serial.print("[WIFI] AP IP: ");
    Serial.println(ip);
}

void Settings::startBluetooth() {
    if (currentConnectivityMode == ConnectivityMode::BT_ONLY) {
        return;
    }

    stopWifiWeb();
    delay(300);

    const char* btName = bt_name_runtime.length() ? bt_name_runtime.c_str() : BT_default;
    bool ok = SerialBT.begin(btName);
    Serial.printf("[BT] begin(%s) = %s\n", btName, ok ? "OK" : "FAIL");

    currentConnectivityMode = ok ? ConnectivityMode::BT_ONLY : ConnectivityMode::NONE;
}

void Settings::stopBluetooth() {
    SerialBT.end();
    delay(200);

    if (currentConnectivityMode == ConnectivityMode::BT_ONLY) {
        currentConnectivityMode = ConnectivityMode::NONE;
    }

    Serial.println("[BT] stopped");
}

void Settings::startWifiWeb() {
    if (currentConnectivityMode == ConnectivityMode::WIFI_WEB) {
        return;
    }

    stopBluetooth();
    delay(300);

    wifiStartup();
    delay(200);

    webStart();

    currentConnectivityMode = ConnectivityMode::WIFI_WEB;
    Serial.println("[WIFI] AP + WEB started");
}

void Settings::stopWifiWeb() {
    if (webServerStarted) {
        _server.end();
        webServerStarted = false;
        Serial.println("[WEB] server stopped");
    }

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(200);

    if (currentConnectivityMode == ConnectivityMode::WIFI_WEB) {
        currentConnectivityMode = ConnectivityMode::NONE;
    }

    Serial.println("[WIFI] AP stopped");
}

void Settings::switchConnectivity(ConnectivityMode newMode) {
    if (newMode == currentConnectivityMode) {
        return;
    }

    switch (newMode) {
        case ConnectivityMode::NONE:
            stopBluetooth();
            stopWifiWeb();
            currentConnectivityMode = ConnectivityMode::NONE;
            break;

        case ConnectivityMode::BT_ONLY:
            startBluetooth();
            break;

        case ConnectivityMode::WIFI_WEB:
            startWifiWeb();
            break;
    }
}

void Settings::init() {
    currentConnectivityMode = ConnectivityMode::NONE;
    webServerStarted = false;
    Serial.println("[SETTINGS] connectivity manager ready");
}

void Settings::begin() {
    _exit = false;
    running = true;
    state = MENU;
    selected = 0;

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
            Serial.println("[SETTINGS] static task create FAIL");
            return;
        }

        Serial.println("[SETTINGS] static task create OK");
    } else {
        vTaskResume(taskHandle);
        Serial.println("[SETTINGS] task resumed");
    }
}

String Settings::readSerialLine() {
    if (Serial.available()) return Serial.readStringUntil('\n');
    if (SerialBT.available()) return SerialBT.readStringUntil('\n');
    return "";
}

void Settings::drawMenu() {
    controls.mutexDisplay([&]() {
        controls.display.clearDisplay();
        controls.display.setTextSize(1);
        controls.display.setTextColor(SH110X_WHITE);

        controls.display.setCursor(0, 0);
        controls.display.println("Nastaveni");

        for (int i = 0; i < menuLength; i++) {
            controls.display.setCursor(0, 16 + i * 12);
            controls.display.print(i == selected ? "> " : " ");
            controls.display.println(menuItems[i]);
        }

        controls.display.display();
    });
}

void Settings::task(void *pvParameters) {
    Settings* self = static_cast<Settings*>(pvParameters);

    for (;;) {
        if (!self->running) {
            vTaskSuspend(nullptr);
        }

        if (self->_exit) {
            self->state = MENU;
            self->selected = 0;
            self->running = false;
            self->switchConnectivity(ConnectivityMode::WIFI_WEB);

            UBaseType_t highWaterWords = uxTaskGetStackHighWaterMark(nullptr);
            Serial.printf("[SETTINGS] task suspended | stack free minimum: %u bytes\n",
                          (unsigned int)(highWaterWords * sizeof(StackType_t)));

            vTaskSuspend(nullptr);
            continue;
        }

        switch (self->state) {
            case MENU:
                self->drawMenu();
                self->Rainbow();

                if (self->controls.leftPressed) {
                    self->selected = (self->selected - 1 + self->menuLength) % self->menuLength;
                    self->controls.leftPressed = false;
                }

                if (self->controls.rightPressed) {
                    self->selected = (self->selected + 1) % self->menuLength;
                    self->controls.rightPressed = false;
                }

                if (self->controls.FingerTouchedFlag()) {
                    if (self->selected == 0) {
                        self->switchConnectivity(ConnectivityMode::WIFI_WEB);
                        self->state = ABOUT;
                    }

                    if (self->selected == 1) {
                        self->switchConnectivity(ConnectivityMode::BT_ONLY);
                        self->state = BT_READ;
                    }

                    if (self->selected == 2) {
                        self->switchConnectivity(ConnectivityMode::BT_ONLY);
                        self->state = WIFI_SSID_READ;
                    }

                    if (self->selected == 3) {
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
        controls.display.setCursor(0, 0);
        controls.display.println("Bluetooth");

        controls.display.setTextSize(1);
        controls.display.setCursor(0, 28);
        controls.display.println("Zadej nove jmeno");

        controls.display.setCursor(0, 54);
        controls.display.println("D16 ZPET");

        controls.display.display();
    });

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

        switchConnectivity(ConnectivityMode::BT_ONLY);

        state = MENU;
        drawMenu();
        return;
    }

    controls.mutexDisplay([&]() {
        controls.display.clearDisplay();
        controls.display.setTextSize(2);
        controls.display.setCursor(0, 0);
        controls.display.println("Bluetooth");

        controls.display.setTextSize(1);
        controls.display.setCursor(0, 26);
        controls.display.print("Novy nazev: ");
        controls.display.println(bt_name_runtime);

        controls.display.setCursor(0, 56);
        controls.display.println("D16 ZPET   D17 OK");

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
        controls.display.setCursor(0, 0);
        controls.display.println("WiFi");

        controls.display.setTextSize(1);
        controls.display.setCursor(0, 28);
        controls.display.println("Zadej SSID");

        controls.display.setCursor(0, 54);
        controls.display.println("D16 ZPET");

        controls.display.display();
    });

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

        switchConnectivity(ConnectivityMode::WIFI_WEB);

        controls.mutexDisplay([&]() {
            controls.display.clearDisplay();
            controls.display.setCursor(0, 0);
            controls.display.println("WiFi AP spusten");
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
        controls.display.setCursor(0, 0);
        controls.display.println("WiFi");

        controls.display.setTextSize(1);
        controls.display.setCursor(0, 26);
        controls.display.print("SSID: ");
        controls.display.println(wifi_ssid_runtime);

        controls.display.setCursor(0, 56);
        controls.display.println("D16 zpet   D17 OK");

        controls.display.display();
    });
}

uint32_t Settings::Wheel(byte WheelPos) {
    WheelPos = 255 - WheelPos;
    if (WheelPos < 85) {
        return controls.strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    }
    if (WheelPos < 170) {
        WheelPos -= 85;
        return controls.strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
    WheelPos -= 170;
    return controls.strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

bool Settings::shouldExit() {
    return _exit;
}

void Settings::drawAbout() {
    if (controls.leftPressed) {
        controls.leftPressed = false;
        state = MENU;
        drawMenu();
        return;
    }

    controls.mutexDisplay([&]() {
        controls.display.clearDisplay();
        controls.display.setTextSize(1);
        controls.display.setCursor(0, 0);
        controls.display.println("O me");

        controls.display.setCursor(0, 12);
        controls.display.print("SSID: ");
        if (wifi_ssid_runtime == "" || wifi_ssid_runtime.length() == 0) {
            controls.display.println(WiFi.softAPSSID());
        } else {
            controls.display.println(wifi_ssid_runtime);
        }

        controls.display.setCursor(0, 26);
        controls.display.print("IP: ");
        controls.display.println(WiFi.softAPIP());

        controls.display.setCursor(0, 40);
        controls.display.print("BT: ");
        if (bt_name_runtime == "" || bt_name_runtime.length() == 0) {
            controls.display.println(BT_default);
        } else {
            controls.display.println(bt_name_runtime);
        }

        controls.display.setCursor(0, 50);
        controls.display.print("MODE: ");
        switch (currentConnectivityMode) {
            case ConnectivityMode::NONE:
                controls.display.println("NONE");
                break;
            case ConnectivityMode::BT_ONLY:
                controls.display.println("BT");
                break;
            case ConnectivityMode::WIFI_WEB:
                controls.display.println("WIFI");
                break;
        }

        controls.display.display();
    });
}

bool Settings::isRunning() {
    return running;
}

void Settings::Rainbow() {
    if (millis() - lastRainbowTime < 50) return;

    lastRainbowTime = millis();

    controls.mutexLed([&]() {
        for (int i = 0; i < controls.strip.numPixels(); i++) {
            controls.strip.setPixelColor(
                i,
                Wheel((i + rainbowPos) & 255)
            );
        }

        controls.strip.show();
    });

    rainbowPos = (rainbowPos + 1) & 255;
}