#pragma once

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

    // Výchozí názvy Bluetooth zařízení a WiFi sítě
    static constexpr const char* BT_default   = "DemoFW";
    static constexpr const char* WIFI_default = "ESP32-Demo";

    Settings(Controls& controlsRef);

    // Inicializace modulu
    void init();
    void begin();

    // Stav modulu
    bool shouldExit();
    bool isRunning();
    void requestStop() { _exit = true; }

    // Vykreslení obrazovek
    void drawAbout();
    void drawDebug();
    void updateRainbow();

    // WiFi a webové rozhraní
    void wifiStartup();
    void webStart();
    AsyncWebServer& server();

    // Ovládání Bluetooth
    void startBluetooth();
    void stopBluetooth();

    // Ovládání WiFi a webserveru
    void startWifiWeb();
    void stopWifiWeb();

    // Přepnutí aktivního komunikačního režimu
    void switchConnectivity(ConnectivityMode newMode);

    String getStoredWiFiSSID() const { return wifi_ssid_runtime; }
    String getStoredBTName() const { return bt_name_runtime; }

private:
    Controls& controls;
    AsyncWebServer _server;

    // Stav komunikace
    ConnectivityMode currentConnectivityMode = ConnectivityMode::WIFI;
    bool webServerStarted = false;

    // FreeRTOS task modulu
    static void task(void *pvParameters);
    TaskHandle_t taskHandle = nullptr;

    // Stav běhu modulu
    volatile bool _exit = false;
    bool running = false;

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

    // Nastavení menu
    static constexpr int visibleItems = 4;
    static constexpr int menuTopOffset = 20;
    static constexpr int menuItemHeight = 12;

    static const int menuLength = 5;
    const String menuItems[menuLength] = { "Info", "Bluetooth", "WiFi", "Debug", "Konec" };

    // Aktuální názvy komunikace
    String bt_name_runtime;
    String wifi_ssid_runtime;

    // Časování animace LED
    unsigned long lastRainbowTime = 0;
    int rainbowPos = 0;

    // Pomocné funkce vykreslení
    void drawMenu();
    void drawBTRead();
    void drawBTConfirm();
    void drawWiFiSSIDRead();
    void drawWiFiConfirm();

    // Pomocné funkce modulu
    void updateMenuSelection(int direction);
    String readSerialLine();
    uint32_t wheelColor(byte wheelPos);
};

extern BluetoothSerial SerialBT;
