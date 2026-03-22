#ifndef RGBMENU_H
#define RGBMENU_H

#include "controls.h"

class RGBMenu {
public:
    RGBMenu(Controls& ctrl);

    void begin();
    void loop();
    void exit();  // nastaví flag pro ukončení
    bool shouldExit();  // vrátí stav flagu

private:
    Controls& controls;

    static const int menuLength = 5;
    const char* menuItems[menuLength] = {"R", "G", "B", "Restart", "Zpet"};

    int sliderValues[3];
    int currentItem;
    bool adjustingValue;
    unsigned long lastChangeTime[3] = {0,0,0};

    // internal UI
    void drawMenu();
    void drawSlider(int x, int y, int w, int h, int value, const char* label, bool selected);
    void updateLED();

    bool _exit;
};

#endif
