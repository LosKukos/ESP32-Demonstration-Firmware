#pragma once

#include <Wire.h>
#include <WiFi.h>
#include <BluetoothSerial.h>
#include <controls.h>

class Settings {
public:
    Settings(Controls& controlsRef);
    void begin();
    void loop();           // Volá se v loop()
    void drawMenu();         // Překreslí hlavní menu
    void showIPOnOLED();     // Zobrazení IP na OLED

    bool shouldExit() const {return _exit;}

private:
    Controls& controls;
    BluetoothSerial SerialBT;

    /* ===== DEFAULTY ===== */
    static constexpr const char* SSID_default = "Mecerodovi";
    static constexpr const char* PASS_default = "sedliste77";
    static constexpr const char* BT_default   = "Test firm";

    /* ===== RUNTIME ===== */
    String bt_name_runtime = "";
    String wifi_ssid_runtime = "";
    String wifi_pass_runtime = "";

    /* ===== MENU ===== */
    static const int menuLength = 3;
    String menuItems[menuLength] = { "Bluetooth", "WiFi", "Zpet" };
    int selected = 0;
    bool _exit = false;
    /* ===== STAVY ===== */
    enum AppState {
        MENU,
        BT_READ,
        BT_CONFIRM,
        WIFI_SSID_READ,
        WIFI_PASS_READ,
        WIFI_CONFIRM,
        EXIT
    };
    AppState state = MENU;

    /* ===== SOUKROMÉ FUNKCE ===== */
    String readSerialLine();
    void drawBTRead();
    void drawBTConfirm();
    void drawWiFiSSIDRead();
    void drawWiFiPassRead();
    void drawWiFiConfirm();
};
