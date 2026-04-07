#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <controls.h>
#include <BluetoothSerial.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

class Settings {
public:
    enum class ConnectivityMode {
        BT,
        WIFI
    };

    Settings(Controls& controlsRef);

    void init();
    void begin();
    bool shouldExit();
    void drawAbout();
    void drawDebug();
    void updateRainbow();
    bool isRunning();

    // Web / WiFi API
    void wifiStartup();
    void webStart();
    AsyncWebServer& server();

    // Connectivity manager
    void startBluetooth();
    void stopBluetooth();

    void startWifiWeb();
    void stopWifiWeb();

    void switchConnectivity(ConnectivityMode newMode);

    void requestStop() { _exit = true; }

    String getStoredWiFiSSID() const { return wifi_ssid_runtime; }
    String getStoredBTName() const { return bt_name_runtime; }

private:
    Controls& controls;
    AsyncWebServer _server;

    static constexpr const char* BT_default   = "Test firm";
    static constexpr const char* WIFI_default = "Bc";

    // Connectivity state
    ConnectivityMode currentConnectivityMode = ConnectivityMode::WIFI;
    bool webServerStarted = false;

    // Task 
    static void task(void *pvParameters);
    TaskHandle_t taskHandle = nullptr;

    // Exit
    volatile bool _exit = false;
    bool running = false;

    // State machine 
    enum State {
        MENU,
        BT_READ,
        BT_CONFIRM,
        WIFI_SSID_READ,
        WIFI_CONFIRM,
        ABOUT,
        DEBUG
    };

    State state = MENU;
    int selected = 0;
    int firstVisibleItem = 0;

    static constexpr int visibleItems = 4;
    static constexpr int menuTopOffset = 20;
    static constexpr int menuItemHeight = 12;

    static const int menuLength = 5;
    const String menuItems[menuLength] = { "Info", "Bluetooth", "WiFi", "Debug", "Konec" };

    // Runtime values
    String bt_name_runtime;
    String wifi_ssid_runtime;

    // Timing 
    unsigned long lastRainbowTime = 0;
    int rainbowPos = 0;

    // Helpers 
    void drawMenu();
    void drawBTRead();
    void drawBTConfirm();
    void drawWiFiSSIDRead();
    void drawWiFiConfirm();

    void updateMenuSelection(int direction);

    String readSerialLine();
    uint32_t wheelColor(byte wheelPos);
};

extern BluetoothSerial SerialBT;

#endif