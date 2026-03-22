#include "settings.h"

// definice globálního BT serialu
BluetoothSerial SerialBT;

Settings::Settings(Controls& controlsRef) : controls(controlsRef) {}

void Settings::start() {
  auto& display = controls.display;
    Serial.begin(115200);
    SerialBT.begin(BT_default);

    WiFi.mode(WIFI_STA);
    delay(100);

    display.clearDisplay();
    display.display();
}

String Settings::readSerialLine() {
    if (Serial.available()) return Serial.readStringUntil('\n');
    if (SerialBT.available()) return SerialBT.readStringUntil('\n');
    return "";
}

void Settings::drawMenu() {
    auto& display = controls.display;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);

    display.setCursor(0, 0);
    display.println("Nastaveni");

    for (int i = 0; i < menuLength; i++) {
        display.setCursor(0, 16 + i * 12);
        display.print(i == selected ? "> " : "  ");
        display.println(menuItems[i]);
    }

    display.display();
}

void Settings::loop() {
    // ===== MENU =====
    if (state == MENU) {
        drawMenu();

        // --- Rainbow efekt ---
        unsigned long now = millis();

        if (now - lastRainbowTime >= 50) {
          lastRainbowTime = now;

          for (int i = 0; i < controls.strip.numPixels(); i++) {
            controls.strip.setPixelColor(i, Wheel((i + rainbowPos) & 255));
          }

          controls.strip.show();
          rainbowPos = (rainbowPos + 1) & 255;
        }

        if (controls.leftPressed || controls.EncoderLeft) {
            selected = (selected - 1 + menuLength) % menuLength;
            controls.leftPressed = false;
            controls.EncoderLeft = false;
            drawMenu();
        }
        if (controls.rightPressed || controls.EncoderRight) {
            selected = (selected + 1) % menuLength;
            controls.rightPressed = false;
            controls.EncoderRight = false;
            drawMenu();
        }
        if (controls.EncoderButton || controls.FingerTouchedFlag()) {
            controls.EncoderButton = false;
            if (selected == 0) state = BT_READ;
            if (selected == 1) state = WIFI_SSID_READ;
            if (selected == 2) state = EXIT;
        }
    }

    if (state == BT_READ) drawBTRead();
    if (state == BT_CONFIRM) drawBTConfirm();
    if (state == WIFI_SSID_READ) drawWiFiSSIDRead();
    if (state == WIFI_PASS_READ) drawWiFiPassRead();
    if (state == WIFI_CONFIRM) drawWiFiConfirm();
    if (state == EXIT){ 
        _exit = true;
    }
    delay(10);
}

/* ===== BT ===== */
void Settings::drawBTRead() {
  auto& display = controls.display;
    if (controls.leftPressed) {
        controls.leftPressed = false;
        state = MENU;
        drawMenu();
        return;
    }

    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("Bluetooth");

    display.setTextSize(1);
    display.setCursor(0, 28);
    display.println("Zadej nove jmeno");

    display.setCursor(0, 54);
    display.println("D16 ZPET");

    display.display();

    String input = readSerialLine();
    if (input.length()) {
        bt_name_runtime = input;
        state = BT_CONFIRM;
    }
}

void Settings::drawBTConfirm() {
  auto& display = controls.display;
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

    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("Bluetooth");

    display.setTextSize(1);
    display.setCursor(0, 26);
    display.print("Novy nazev: ");
    display.println(bt_name_runtime);

    display.setCursor(0, 56);
    display.println("D16 ZPET   D17 OK");

    display.display();
}

/* ===== WIFI ===== */
void Settings::drawWiFiSSIDRead() {
  auto& display = controls.display;
    if (controls.leftPressed) {
        controls.leftPressed = false;
        state = MENU;
        drawMenu();
        return;
    }

    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("WiFi");

    display.setTextSize(1);
    display.setCursor(0, 28);
    display.println("Zadej SSID");

    display.setCursor(0, 54);
    display.println("D16 ZPET");

    display.display();

    String input = readSerialLine();
    if (input.length()) {
        wifi_ssid_runtime = input;
        state = WIFI_PASS_READ;
    }
}

void Settings::drawWiFiPassRead() {
  auto& display = controls.display;
    if (controls.leftPressed) {
        controls.leftPressed = false;
        state = MENU;
        drawMenu();
        return;
    }

    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("WiFi");

    display.setTextSize(1);
    display.setCursor(0, 28);
    display.println("Zadej HESLO");

    display.setCursor(0, 54);
    display.println("D16 ZPET");

    display.display();

    String input = readSerialLine();
    if (input.length()) {
        wifi_pass_runtime = input;
        state = WIFI_CONFIRM;
    }
}

void Settings::drawWiFiConfirm() {
  auto& display = controls.display;
    if (controls.leftPressed) {
        controls.leftPressed = false;
        state = MENU;
        drawMenu();
        return;
    }

    if (controls.rightPressed) {
        controls.rightPressed = false;
        WiFi.begin(wifi_ssid_runtime.c_str(), wifi_pass_runtime.c_str());

        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Pripojuji WiFi...");
        display.display();

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
            delay(200);
        }

        if (WiFi.status() == WL_CONNECTED) showIPOnOLED();
        state = MENU;
        drawMenu();
        return;
    }

    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("WiFi");

    display.setTextSize(1);
    display.setCursor(0, 26);
    display.print("SSID: ");
    display.println(wifi_ssid_runtime);

    display.setCursor(0, 36);
    display.print("Heslo: ");
    display.println(wifi_pass_runtime);

    display.setCursor(0, 56);
    display.println("D16 zpet   D17 OK");

    display.display();
}

void Settings::showIPOnOLED() {
  auto& display = controls.display;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);

    display.println("WiFi pripojeno");
    display.println();
    display.print("IP:");
    display.println(WiFi.localIP());

    display.display();
    delay(5000);

    display.clearDisplay();
    display.display();
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
    digitalWrite(controls.IR_LED_PIN, LOW);
    digitalWrite(controls.RED_LED_PIN, LOW);
}

void Settings::begin() {
    _exit = false;
    state = MENU;
}