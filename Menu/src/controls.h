#pragma once

// Knihovny
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_NeoPixel.h>

// Rozměry displeje
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64


class Controls {
public:
    Controls();

    // Přiřazení hodnot k pinům
    static constexpr int BUZZER_PIN = 2;
    static constexpr int LEFT_PIN = 16;
    static constexpr int RIGHT_PIN = 17;
    static constexpr int TOUCH_PIN = 4;

    static constexpr int PPG_PIN = 39;
    static constexpr int IR_LED_PIN = 14;
    static constexpr int RED_LED_PIN = 15;

    static constexpr int NUM_LEDS = 1;
    static constexpr int LED_PIN = 27;

    // Funkce pro inicializaci a ovládání ovládacích prvků
    void begin();
    bool fingerTouched();
    bool FingerTouchedFlag();
    void resetTouchState();
    void mutexDisplay(std::function<void()> fn);
    void mutexLed(std::function<void()> fn);
    void calibrateTouch();

    volatile bool leftPressed = false;
    volatile bool rightPressed = false;

    Adafruit_SH1106G display;
    Adafruit_NeoPixel strip;

private:

    // Proměnné pro detekci dotyku a debouncing
    static constexpr unsigned long debounceTime = 250;
    int touch_baseline;
    volatile unsigned long lastLeftTime;
    volatile unsigned long lastRightTime;
    bool touchState = false;           
    bool prevTouchFlagState = false;    
    unsigned long lastTouchChangeTime = 0;

    // Funkce pro zpracování stisků tlačítek a inicializaci displeje
    void initDisplay();
    void handleLeft();
    void handleRight();

    SemaphoreHandle_t displayMutex;
    SemaphoreHandle_t ledMutex;

    static Controls* instance;
};