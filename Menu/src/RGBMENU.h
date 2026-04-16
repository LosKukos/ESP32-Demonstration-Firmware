#ifndef RGBMENU_H
#define RGBMENU_H

#include <Arduino.h>
#include <controls.h>
#include <settings.h>

class RGBMenu {
public:
    explicit RGBMenu(Controls& ctrl, Settings& settingsRef);

    void begin();
    bool shouldExit();
    bool isRunning();
    void requestStop();
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

    static constexpr uint8_t menuLength = 5;
    const char* menuItems[menuLength] = {"R", "G", "B", "Reset", "Konec"};

    TaskHandle_t taskHandle = nullptr;
    SemaphoreHandle_t dataMutex = nullptr;
    bool webRoutesRegistered = false;
    bool running = false;

    static void task(void* pvParameters);

    void handleTouch();
    void drawMenu(const State& snapshot);
    void drawSlider(int x, int y, int w, int h, int value, const char* label, bool selected);
    void applyLedColor(uint8_t r, uint8_t g, uint8_t b);
    State copyState();
    void resetState(bool requestExit);
};

#endif