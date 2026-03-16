#ifndef SETTINGS_H
#define SETTINGS_H

#pragma once

#include <Wire.h>
#include <WiFi.h>
#include <BluetoothSerial.h>
#include "controls.h"

// globální BT serial (přístupné z jiných modulů)
extern BluetoothSerial SerialBT;

extern unsigned long lastRainbowTime;
extern int rainbowPos;

class Settings {
public:
    Settings(Controls& controlsRef);
    void begin();          // Volá se při vstupu do nastavení
    void loop();           // Volá se v loop()
    void drawMenu();         // Překreslí hlavní menu
    bool shouldExit();  // vrácení stavu pro návrat
    void start(); // Volá se v setup() pro inicializaci nastavení
    void Wifi_startup(); // Spuštění WiFi přístupového bodu s výchozím SSID a heslem
    void debug();

private:
    Controls& controls;
    BluetoothSerial SerialBT;

    /* ===== DEFAULTY ===== */
    static constexpr const char* SSID_default = "ESP32-AP";
    static constexpr const char* PASS_default = "1234567890";
    static constexpr const char* BT_default   = "Test firm";

    /* ===== RUNTIME ===== */
    String bt_name_runtime = "";
    String wifi_ssid_runtime = "";
    String wifi_pass_runtime = "";

    /* ===== MENU ===== */
    static const int menuLength = 3;
    String menuItems[menuLength] = { "Bluetooth", "WiFi", "Zpet"};
    int selected = 0;
    bool _exit = false;

    /* ===== RAINBOW ===== */
    unsigned long lastRainbowTime = 0; 
    uint8_t rainbowPos = 0;            

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

    /* ===== RAINBOW HELP ===== */
    uint32_t Wheel(byte WheelPos);
};

#endif
