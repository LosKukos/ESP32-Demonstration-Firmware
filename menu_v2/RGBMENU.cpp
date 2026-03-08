#include "rgbmenu.h"

RGBMenu::RGBMenu(Controls& ctrl) : controls(ctrl), currentItem(0), adjustingValue(false)
{
    sliderValues[0] = 0;
    sliderValues[1] = 0;
    sliderValues[2] = 0;

    lastChangeTime[0] = lastChangeTime[1] = lastChangeTime[2] = 0;
}

void RGBMenu::begin() {
    currentItem = 0;
    _exit = false;
    controls.strip.setPixelColor(0, 0, 0, 0);
    controls.strip.show();
}

void RGBMenu::loop() {
    unsigned long now = millis();

    // ------------------------------------------------
    // Přepínání mezi slidery a navigací v menu
    // ------------------------------------------------
    if ((controls.FingerTouchedFlag() || controls.EncoderButton) && currentItem < 3) {
        controls.EncoderButton = false;
        adjustingValue = !adjustingValue;
    }

    // ------------------------------------------------
    // Menu navigace
    // ------------------------------------------------
    if (!adjustingValue) {
        if (controls.EncoderLeft || controls.leftPressed) {
            controls.EncoderLeft = false;
            controls.leftPressed = false;
            currentItem = (currentItem - 1 + menuLength) % menuLength;
        }
        if (controls.EncoderRight || controls.rightPressed) {
            controls.EncoderRight = false;
            controls.rightPressed = false;
            currentItem = (currentItem + 1) % menuLength;
        }
    }

    // ------------------------------------------------
    // Levý enkódér a tlačítko pro posun doleva
    // ------------------------------------------------
    if ((controls.EncoderLeft || controls.leftPressed) && adjustingValue && currentItem < 3) {
        controls.EncoderLeft = false;
        controls.leftPressed = false;  
        unsigned long dt = now - lastChangeTime[currentItem];

        // Exponenciální akcelerace enkódéru
        int step = 1;
        if (dt < 200) step = 2;
        if (dt < 150) step = 3;
        if (dt < 100) step = 4;
        if (dt < 60)  step = 6;
        if (dt < 40)  step = 8;

        sliderValues[currentItem] -= step;
        if (sliderValues[currentItem] < 0) sliderValues[currentItem] = 0;

        lastChangeTime[currentItem] = now;
    }

    // ------------------------------------------------
    // Pravý enkódér a tlačítko pro posun doleva
    // ------------------------------------------------
    if ((controls.EncoderRight || controls.rightPressed)  && adjustingValue && currentItem < 3) {
        controls.EncoderRight = false;
        controls.rightPressed = false;
        unsigned long dt = now - lastChangeTime[currentItem];

        // Exponenciální akcelerace
        int step = 1;
        if (dt < 200) step = 2;
        if (dt < 150) step = 3;
        if (dt < 100) step = 4;
        if (dt < 60)  step = 6;
        if (dt < 40)  step = 8;

        sliderValues[currentItem] += step;
        if (sliderValues[currentItem] > 255) sliderValues[currentItem] = 255;

        lastChangeTime[currentItem] = now;
    }

    // ------------------------------------------------
    // Potvrzení 
    // ------------------------------------------------
    if (currentItem >= 3 && (controls.fingerTouched() || controls.EncoderButton)) {
        controls.EncoderButton = false;
        adjustingValue = false;

        // Reset sliderů při Restartu nebo Exit
        sliderValues[0] = sliderValues[1] = sliderValues[2] = 0;

        if (currentItem == 4) {  // Zpet / Exit
            _exit = true;
        }
    }

    updateLED();
    drawMenu();
}


// ------------------------------------------------
// UI
// ------------------------------------------------

void RGBMenu::drawSlider(int x, int y, int w, int h, int value, const char* label, bool selected) {
    auto& display = controls.display;

    display.drawRect(x, y, w, h, SH110X_WHITE);
    display.fillRect(x, y, map(value, 0, 255, 0, w), h, SH110X_WHITE);

    display.setCursor(x + w + 4, y);
    display.print(label);
    display.print(" ");
    display.print(value);

    if (selected) {
        display.setCursor(x - 6, y);
        display.print(">");
    }
}

void RGBMenu::drawMenu() {
    auto& display = controls.display;
    display.clearDisplay();

    int yOffset = 2;
    int spacing = 10;

    for (int i = 0; i < menuLength; i++) {
        if (i < 3) {
            drawSlider(5, yOffset + i * spacing, 80, 8, sliderValues[i], menuItems[i], i == currentItem);
        } else {
            display.setCursor(5, yOffset + i * spacing + 10);
            if (i == currentItem) display.print(">");
            display.print(menuItems[i]);
        }
    }

    // HEX value
    display.setCursor(5, yOffset + 3 * spacing);
    char hexColor[7];
    sprintf(hexColor, "%02X%02X%02X", sliderValues[0], sliderValues[1], sliderValues[2]);
    display.print("HEX: #");
    display.print(hexColor);

    display.display();
}

void RGBMenu::updateLED() {
    controls.strip.setPixelColor(0, sliderValues[0], sliderValues[1], sliderValues[2]);
    controls.strip.show();
}

bool RGBMenu::shouldExit() {
    return _exit;
}

void RGBMenu::exit() {
    _exit = true;
}