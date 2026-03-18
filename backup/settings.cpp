#include "settings.h"

// definice globálního BT serialu
BluetoothSerial SerialBT;

Settings::Settings(Controls& controlsRef) : controls(controlsRef) {}

void Settings::start() {
    SerialBT.begin(BT_default); // Spuštění Bluetooth s výchozím názvem
    controls.display.clearDisplay(); // Vyčistění OLED displaye
    controls.display.display();
}

String Settings::readSerialLine() {
    if (Serial.available()) return Serial.readStringUntil('\n');
    if (SerialBT.available()) return SerialBT.readStringUntil('\n');
    return "";
}

void Settings::drawMenu() {
    controls.display.clearDisplay();
    controls.display.setTextSize(1);
    controls.display.setTextColor(SH110X_WHITE);

    controls.display.setCursor(0, 0);
    controls.display.println("Nastaveni");

    for (int i = 0; i < menuLength; i++) { 
        controls.display.setCursor(0, 16 + i * 12); 
        controls.display.print(i == selected ? "> " : " "); 
        controls.display.println(menuItems[i]); }

    controls.display.setCursor(0, 16 + 3 * 12);
    controls.display.print(" ");
    controls.display.print("IP: ");
    controls.display.println(WiFi.softAPIP());

    controls.display.display();
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

        if (controls.leftPressed) {
            selected = (selected - 1 + menuLength) % menuLength;
            controls.leftPressed = false;
            drawMenu();
        }
        if (controls.rightPressed) {
            selected = (selected + 1) % menuLength;
            controls.rightPressed = false;
            drawMenu();
        }
        if (controls.FingerTouchedFlag()) {
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
    if (controls.leftPressed) {
        controls.leftPressed = false;
        state = MENU;
        drawMenu();
        return;
    }

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
}

/* ===== WIFI ===== */
void Settings::drawWiFiSSIDRead() {
    if (controls.leftPressed) {
        controls.leftPressed = false;
        state = MENU;
        drawMenu();
        return;
    }

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

    String input = readSerialLine();
    input.trim();

    if (input.length()) {
        wifi_ssid_runtime = input;
        state = WIFI_PASS_READ;
    }
}

void Settings::drawWiFiPassRead() {
    if (controls.leftPressed) {
        controls.leftPressed = false;
        state = MENU;
        drawMenu();
        return;
    }

    controls.display.clearDisplay();
    controls.display.setTextSize(2);
    controls.display.setCursor(0, 0);
    controls.display.println("WiFi");

    controls.display.setTextSize(1);
    controls.display.setCursor(0, 28);
    controls.display.println("Zadej HESLO");

    controls.display.setCursor(0, 54);
    controls.display.println("D16 ZPET");

    controls.display.display();

    String input = readSerialLine();
    input.trim();

    if (input.length() >= 8) {
        wifi_pass_runtime = input;
        state = WIFI_CONFIRM;
    } else if (input.length() > 0 && input.length() < 8) {
        controls.display.setCursor(0, 40);
        controls.display.setTextColor(SH110X_WHITE);
        controls.display.println("Heslo musi mit alespon 8 znaku");
        controls.display.setTextColor(SH110X_WHITE);
        controls.display.display();
        delay(1500);
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
    WiFi.softAP(wifi_ssid_runtime.c_str(), wifi_pass_runtime.c_str());

    controls.display.clearDisplay();
    controls.display.setCursor(0, 0);
    controls.display.println("WiFi AP spusten");
    controls.display.print("IP: ");
    controls.display.println(WiFi.softAPIP());
    controls.display.display();

    delay(1500);

    state = MENU;
    drawMenu();
    return;
    }

    controls.display.clearDisplay();
    controls.display.setTextSize(2);
    controls.display.setCursor(0, 0);
    controls.display.println("WiFi");

    controls.display.setTextSize(1);
    controls.display.setCursor(0, 26);
    controls.display.print("SSID: ");
    controls.display.println(wifi_ssid_runtime);

    controls.display.setCursor(0, 36);
    controls.display.print("Heslo: ");
    controls.display.println(wifi_pass_runtime);

    controls.display.setCursor(0, 56);
    controls.display.println("D16 zpet   D17 OK");

    controls.display.display();
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

void Settings::begin() {
    _exit = false;
    state = MENU;
    selected = 0;

}

void Settings::Wifi_startup() {

    controls.display.clearDisplay();
    controls.display.setTextColor(SH110X_WHITE);
    controls.display.setTextSize(1);
    controls.display.setCursor(0,0);
    controls.display.println("Spoustim AP...");
    controls.display.display();

    WiFi.mode(WIFI_AP);
    WiFi.softAP(SSID_default, PASS_default);

    // počkej, až AP dostane IP
    IPAddress ip;
    unsigned long start = millis();
    do {
        ip = WiFi.softAPIP();
        delay(50);
    } while(ip == IPAddress(0,0,0,0) && millis() - start < 5000); // 5s místo 3s

    delay(500);
    
    // vypiš na OLED
    controls.display.clearDisplay();
    controls.display.setTextColor(SH110X_WHITE);
    controls.display.setTextSize(1);
    controls.display.setCursor(0, 0);
    controls.display.println("AP spusten");
    controls.display.setCursor(0, 26);
    controls.display.print("IP: ");
    controls.display.println(ip);
    controls.display.display();
}