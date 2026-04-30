#pragma once

// Knihovny
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_NeoPixel.h>

// Rozměry OLED displeje
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

class Controls {
public:
    Controls();

    // Pinové přiřazení
    static constexpr int BUZZER_PIN = 2;
    static constexpr int LEFT_PIN = 16;
    static constexpr int RIGHT_PIN = 17;
    static constexpr int TOUCH_PIN = 4;

    static constexpr int PPG_PIN = 39;
    static constexpr int IR_LED_PIN = 14;
    static constexpr int RED_LED_PIN = 15;

    static constexpr int NUM_LEDS = 1;
    static constexpr int LED_PIN = 27;

    // Inicializace modulu
    void begin();

    // Detekce dotyku
    bool fingerTouched();
    bool FingerTouchedFlag();
    void resetTouchState();
    void calibrateTouch();

    // Uzamčení displeje a LED diody
    void mutexDisplay(std::function<void()> fn);
    void mutexLed(std::function<void()> fn);

    // Stav tlačítek
    volatile bool leftPressed = false;
    volatile bool rightPressed = false;

    // Objekty displeje a adresovatelné LED
    Adafruit_SH1106G display;
    Adafruit_NeoPixel strip;

private:
    // Debounce tlačítek
    static constexpr unsigned long debounceTime = 250;

    // Proměnné pro vyhodnocení dotyku
    int touch_baseline;
    bool touchState = false;           
    bool prevTouchFlagState = false;    
    unsigned long lastTouchChangeTime = 0;

    // Časy posledního stisku tlačítek
    volatile unsigned long lastLeftTime;
    volatile unsigned long lastRightTime;

    // Pomocné funkce modulu
    void initDisplay();
    void handleLeft();
    void handleRight();

    // Mutexy pro displej a LED diodu
    SemaphoreHandle_t displayMutex;
    SemaphoreHandle_t ledMutex;

    // Instance pro použití v přerušení
    static Controls* instance;
};
