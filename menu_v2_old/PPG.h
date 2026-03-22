#pragma once
#include <Arduino.h>
#include <controls.h>

class PPG {
public:
    enum LedMode { IR, RED };
    enum SystemState { MEASURING, MENU, CALIBRATION };

    PPG(Controls& ctrl);

    void start();
    void loop();
    void begin();

    bool shouldExit();

private:
    Controls& controls;

    // --- PPG proměnné ---
    int ambientBaseline;
    int currentValue;
    const unsigned long ppgReadInterval = 20;
    unsigned long lastPPGReadTime;

    // --- FIFO buffer ---
    static const int bufferSize = 128;
    int ppgBuffer[bufferSize];
    int bufferIndex;

    // --- Filtrace PPG ---
    float emaValue;
    const float alpha = 0.2;
    int smaBuffer[5];
    int smaIndex;

    // --- BPM výpočet ---
    unsigned long lastPeakTime;
    int currentBPM;
    bool bpmIsValid;

    // --- Menu stav ---
    bool menuActive;
    int menuSelection;
    static const int menuItemCount = 4;
    const char* menuItems[menuItemCount] = { "Kalibrace", "Autozoom", "Zpet", "Konec" };

    // --- LED mód ---
    LedMode currentMode;
    SystemState currentState;

    // --- Thresholdy ---
    int adjThreshold;
    const int minBeatInterval = 300;
    int touch_baseline;

    // --- Display ---
    const int bpmX = 0;
    const int bpmY = 0;
    bool autoZoomEnabled;

    // --- BPM buffer ---
    static const int intervalBufferSize = 4;
    unsigned long intervalBuffer[intervalBufferSize];
    int intervalIndex;

    // --- Globální proměnné ---
    const unsigned long fingerHoldTime = 250;
    unsigned long lastPeakTime_LED;
    const unsigned long blinkDuration = 50;
    bool ledOn;
    bool peakFlag;
    unsigned long lastBpmSendTime;
    int lastSentBpm;
    int width = 128;
    int height = 64;

    // --- Funkce ---
    void sampleAndProcessPPG();
    void handleStateSwitch();
    void handleLedSwitch();
    bool detectPeak(int currentValue, int threshold);
    void calculateBPM(unsigned long interval);
    void calibrateSensor();
    int ReadPPG();
    void drawPPGCurve();
    void displayBPM();
    int calculateDynamicThreshold();
    void drawMenu();
    void blinkPeak(bool peak);
    void calibrateTouch();
    bool fingerTouched();
    bool fingerMenuPressed();

    bool _exit;
};