#ifndef CONTROLS_H
#define CONTROLS_H

// --- Knihovny ---
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_NeoPixel.h>

// --- Rozlišení OLED displeje ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

class Controls {
public:
    Controls();

    // --- Obecné piny ---
    const int BUZZER_PIN = 2;
    const int LEFT_PIN = 16;
    const int RIGHT_PIN = 17;
    const int TOUCH_PIN = 4;

    // --- PPG piny ---
    const int PPG_PIN = 39;
    const int IR_LED_PIN = 14;
    const int RED_LED_PIN = 15;

    // --- Piny WS2812 LED ---
    const int NUM_LEDS = 1;
    const int LED_PIN = 27;

    // --- Piny enkódéru ---
    const int ENC_CLK_PIN = 32;
    const int ENC_DT_PIN = 33;
    const int ENC_SW_PIN = 12;

    // --- Funkce ---
    void begin();              // Inicializace periferií
    bool fingerTouched();      // Kontrola dotyku prstu
    bool FingerTouchedFlag();  // Flag pro použití kódem
    
    // --- Input/Output ---
    volatile bool leftPressed = false;
    volatile bool rightPressed = false;
    volatile bool EncoderButton = false;
    volatile bool EncoderLeft = false;
    volatile bool EncoderRight = false;

    // Konstruktory/objekty
    Adafruit_SH1106G display;
    Adafruit_NeoPixel strip;

private:

    // --- Definice proměnných ---
    const unsigned long debounceTime = 250;
    const unsigned long debounceTimeEnc = 2;
    int touch_baseline;
    volatile unsigned long lastLeftTime;
    volatile unsigned long lastRightTime;
    volatile unsigned long lastEncButtonTime;

    // --- Funkce ---
    void initDisplay();
    void calibrateTouch();

    // --- Interrupty ---
    void IRAM_ATTR handleLeft();
    void IRAM_ATTR handleRight();
    void IRAM_ATTR handleEncoder();
    void IRAM_ATTR handleEncoderButton();

    static Controls* instance;
};

#endif