#include "settings.h"

BluetoothSerial SerialBT;

Settings::Settings(Controls& controlsRef) : controls(controlsRef) {}

void Settings::init(){
    SerialBT.begin(BT_default);
}

void Settings::begin() {

    _exit = false;
    state = MENU;
    selected = 0;

    xTaskCreatePinnedToCore(
        task,
        "SettingsTask",
        4096,
        this,
        1,
        &taskHandle,
        1
    );
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

        if (self->_exit) {
            break;
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
                    if (self->selected == 0) self->state = BT_READ;
                    if (self->selected == 1) self->state = WIFI_SSID_READ;
                    if (self->selected == 2) self->state = ABOUT;
                    if (self->selected == 3) self->_exit = true;
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

        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
    self->taskHandle = NULL;
    vTaskDelete(NULL);
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

        SerialBT.end();
        delay(50);
        SerialBT.begin(bt_name_runtime);

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

        WiFi.softAPdisconnect(true);
        delay(100);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(wifi_ssid_runtime.c_str(), NULL);

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

        // SSID
        controls.display.setCursor(0, 12);
        controls.display.print("SSID: ");
        if (wifi_ssid_runtime == "" || wifi_ssid_runtime.length() == 0) {
            controls.display.println(WiFi.softAPSSID());
        } else {
            controls.display.println(wifi_ssid_runtime);
        }

        // IP
        controls.display.setCursor(0, 26);
        controls.display.print("IP: ");
        controls.display.println(WiFi.softAPIP());

        // BT
        controls.display.setCursor(0, 40);
        controls.display.print("BT: ");
        if (bt_name_runtime == "" || bt_name_runtime.length() == 0) {
            controls.display.println(BT_default);
        } else {
            controls.display.println(bt_name_runtime);
        }

        controls.display.setCursor(0, 56);
        controls.display.println("D16 ZPET");

        controls.display.display();
    });
}

bool Settings::isRunning() {
    return taskHandle != NULL;
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