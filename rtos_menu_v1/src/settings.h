#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <controls.h>
#include <BluetoothSerial.h>
#include <WiFi.h>

class Settings {
public:
    Settings(Controls& controlsRef);
    void init();
    void begin();
    bool shouldExit();
    void drawAbout();
    void Rainbow();
    bool isRunning();

private:
    Controls& controls;

    static constexpr const char* BT_default   = "Test firm";

    // ===== Task =====
    static void task(void *pvParameters);
    TaskHandle_t taskHandle = nullptr;

    // ===== Exit =====
    volatile bool _exit = false;

    // ===== State machine =====
    enum State {
        MENU,
        BT_READ,
        BT_CONFIRM,
        WIFI_SSID_READ,
        WIFI_CONFIRM,
        ABOUT
    };

    State state = MENU;

    int selected = 0;

    static const int menuLength = 4;
    const String menuItems[menuLength] = { "Bluetooth", "WiFi","Info", "Zpet" };

    // ===== Runtime values =====
    String bt_name_runtime;
    String wifi_ssid_runtime;
    String wifi_pass_runtime;

    // ===== Timing =====
    unsigned long lastRainbowTime = 0;
    int rainbowPos = 0;

    // ===== Helpers =====
    void drawMenu();
    void drawBTRead();
    void drawBTConfirm();
    void drawWiFiSSIDRead();
    void drawWiFiConfirm();

    String readSerialLine();
    uint32_t Wheel(byte WheelPos);
};

extern BluetoothSerial SerialBT;
#endif