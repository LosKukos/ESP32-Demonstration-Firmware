#pragma once

#include <Arduino.h>
#include <controls.h>
#include <settings.h>

class RGBMenu {
public:
    explicit RGBMenu(Controls& ctrl, Settings& settingsRef);

    // Inicializace modulu
    void begin();

    // Stav modulu
    bool shouldExit();
    bool isRunning();
    void requestStop();

    // Webové rozhraní
    void registerWebRoutes();

private:
    struct State {
        uint8_t rgb[3] = {0, 0, 0};
        uint8_t currentItem = 0;
        bool adjustingValue = false;
        bool exitRequested = false;
        bool dirty = true;
    };

    Controls& controls;
    Settings& settings;
    State state;

    // Položky menu
    static constexpr uint8_t menuLength = 5;
    const char* menuItems[menuLength] = {"R", "G", "B", "Reset", "Konec"};

    // FreeRTOS task modulu
    TaskHandle_t taskHandle = nullptr;
    static void task(void* pvParameters);

    // Sdílená data modulu
    SemaphoreHandle_t dataMutex = nullptr;
    bool webRoutesRegistered = false;
    bool running = false;

    // Zpracování vstupů
    void handleTouch();

    // Vykreslení rozhraní
    void drawMenu(const State& snapshot);
    void drawSlider(int x, int y, int w, int h, int value, const char* label, bool selected);

    // Ovládání LED diody
    void applyLedColor(uint8_t r, uint8_t g, uint8_t b);

    // Práce se stavem modulu
    State copyState();
    void resetState(bool requestExit);
};
