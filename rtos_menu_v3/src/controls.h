#ifndef CONTROLS_H
#define CONTROLS_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_NeoPixel.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

class Controls {
public:
    Controls();

    const int BUZZER_PIN = 2;
    const int LEFT_PIN = 16;
    const int RIGHT_PIN = 17;
    const int TOUCH_PIN = 4;

    const int PPG_PIN = 39;
    const int IR_LED_PIN = 14;
    const int RED_LED_PIN = 15;

    const int NUM_LEDS = 1;
    const int LED_PIN = 27;

    void begin();
    bool fingerTouched();
    bool FingerTouchedFlag();
    void resetTouchState();              // <- přidat
    void mutexDisplay(std::function<void()> fn);
    void mutexLed(std::function<void()> fn);

    volatile bool leftPressed = false;
    volatile bool rightPressed = false;

    Adafruit_SH1106G display;
    Adafruit_NeoPixel strip;

private:
    const unsigned long debounceTime = 250;
    int touch_baseline;
    volatile unsigned long lastLeftTime;
    volatile unsigned long lastRightTime;

    bool touchState = false;             // <- přidat
    bool prevTouchFlagState = false;     // <- přidat
    unsigned long lastTouchChangeTime = 0; // <- přidat

    void initDisplay();
    void calibrateTouch();

    void handleLeft();
    void handleRight();

    SemaphoreHandle_t displayMutex;
    SemaphoreHandle_t ledMutex;

    static Controls* instance;
};

#endif