#ifndef RGBMENU_H
#define RGBMENU_H

#include <Arduino.h>
#include <controls.h>
#include <web.h>

class RGBMenu {
public:
    RGBMenu(Controls& ctrl);

    void start();
    bool shouldExit();
    bool isRunning();
    void web_RGBMenu();

private:
    Controls& controls;

    // ===== Task =====
    static void task(void *pvParameters);
    TaskHandle_t taskHandle = nullptr;
    SemaphoreHandle_t dataMutex;

    // ===== State =====
    static const int menuLength = 5;
    const char* menuItems[menuLength] = {"R", "G", "B", "Restart", "Zpet"};

    volatile uint8_t sliderValues[3] = {0, 0, 0};
    volatile int pendingValues[3] = {0,0,0};
    volatile bool pendingUpdate = false;
    int currentItem = 0;
    bool adjustingValue = false;
    volatile bool _exit = false;
    volatile bool needsUpdate = false;

    // ===== UI =====
    void drawMenu();
    void drawSlider(int x, int y, int w, int h, int value, const char* label, bool selected);
    void updateLED();

    // ===== Touch handler (missing before) =====
    void handleTouch();
};

#endif