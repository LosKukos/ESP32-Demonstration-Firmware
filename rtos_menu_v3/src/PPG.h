#ifndef PPG_H
#define PPG_H

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <controls.h>
#include <settings.h>

class PPG {
public:
    enum LedMode { IR, RED };
    enum SystemState { MEASURING, MENU, CALIBRATION };

    explicit PPG(Controls& ctrl, Settings& settingsRef);

    void begin();
    bool shouldExit();
    bool isRunning();
    void calibrateSensor();
    void web_ppg();

    TaskHandle_t getTaskHandle() const { return taskHandle; }
    void clearTaskHandle() { taskHandle = nullptr; }
    void requestStop() { _exit = true; }

private:
    enum MenuItem {
        MENU_CALIBRATION = 0,
        MENU_AUTOZOOM,
        MENU_BACK,
        MENU_EXIT
    };

    Controls& controls;
    Settings& settings;
    static void task(void* pvParameters);

    TaskHandle_t taskHandle = nullptr;
    SemaphoreHandle_t dataMutex = nullptr;
    volatile bool _exit = false;
    bool running = false;

    // Stav modulu
    SystemState currentState = MEASURING;
    LedMode currentMode = IR;
    int menuSelection = 0;
    bool autoZoomEnabled = false;

    // Mereni
    static constexpr unsigned long PPG_READ_INTERVAL_MS = 20;
    static constexpr unsigned long MIN_BEAT_INTERVAL_MS = 300;
    static constexpr int BPM_MIN_VALID = 40;
    static constexpr int BPM_MAX_VALID = 200;
    static constexpr int BPM_MAX_JUMP = 35;
    static constexpr int BUFFER_SIZE = 128;
    static constexpr int SMA_SIZE = 5;
    static constexpr int INTERVAL_BUFFER_SIZE = 4;
    static constexpr float EMA_ALPHA = 0.2f;

    int ambientBaseline = 0;
    int currentValue = 0;
    unsigned long lastPPGReadTime = 0;

    int ppgBuffer[BUFFER_SIZE] = {0};
    int bufferIndex = 0;

    float emaValue = 0.0f;
    int smaBuffer[SMA_SIZE] = {0};
    int smaIndex = 0;
    int smaCount = 0;

    unsigned long intervalBuffer[INTERVAL_BUFFER_SIZE] = {0};
    int intervalIndex = 0;
    unsigned long lastPeakMillis = 0;
    bool signalAboveThreshold = false;
    int adjThreshold = 0;

    // Sdilena data pro web/UI
    int currentBPM = 0;
    int lastAcceptedBPM = 0;
    bool bpmIsValid = false;

    // UI / indikace
    bool fingerWasPresent = false;
    unsigned long lastFingerLostTime = 0;
    static constexpr unsigned long FINGER_LOST_POPUP_MS = 1200;

    bool ledOn = false;
    unsigned long lastPeakLedTime = 0;
    static constexpr unsigned long BLINK_DURATION_MS = 50;

    static constexpr int DISPLAY_WIDTH = 128;
    static constexpr int DISPLAY_HEIGHT = 64;

    static constexpr int menuItemCount = 4;
    const char* menuItems[menuItemCount] = {
        "Kalibrace",
        "Autozoom",
        "Zpet",
        "Konec"
    };

    void resetMeasurementState();
    void sampleAndProcessPPG();
    void renderMeasurementScreen(bool fingerOn);
    void handleStateSwitch();
    void handleLedSwitch();
    bool detectPeak(int value, int threshold, unsigned long now);
    void calculateBPM(unsigned long interval);
    bool isBpmPlausible(int bpm) const;
    int readPPG();
    void drawPPGCurve(Adafruit_SH1106G& display);
    void displayBPM(Adafruit_SH1106G& display, bool fingerOn);
    int calculateDynamicThreshold() const;
    void drawMenu();
    void blinkPeak(bool peak);
    void lockData();
    void unlockData();
};

#endif