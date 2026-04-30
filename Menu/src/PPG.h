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

    // Inicializace modulu
    void begin();

    // Stav modulu
    bool shouldExit();
    bool isRunning();
    void requestStop() { _exit = true; }

    // Kalibrace fotosenzoru
    void calibrateSensor();

    // Webové rozhraní
    void registerWebRoutes();

private:
    enum MenuItem {
        MENU_CALIBRATION = 0,
        MENU_AUTOZOOM,
        MENU_BACK,
        MENU_EXIT
    };

    Controls& controls;
    Settings& settings;

    // FreeRTOS task modulu
    static void task(void* pvParameters);
    TaskHandle_t taskHandle = nullptr;

    // Sdílená data modulu
    SemaphoreHandle_t dataMutex = nullptr;
    volatile bool _exit = false;
    bool running = false;

    // Stav modulu
    SystemState currentState = MEASURING;
    LedMode currentMode = IR;
    int menuSelection = 0;
    bool autoZoomEnabled = false;

    // Nastavení měření
    static constexpr unsigned long ppgInterval = 20;
    static constexpr unsigned long beatInterval = 300;
    static constexpr int bpmValidMin = 40;
    static constexpr int bpmValidMax = 200;
    static constexpr int bpmDiff = 35;
    static constexpr int bufferSize = 128;
    static constexpr int intervalBufferSize = 3;
    static constexpr float filterWeight = 0.4f;

    // Výchozí úroveň signálu pro zobrazení
    static constexpr int displayBaselineSampleCount = 150;
    long displayBaselineSum = 0;
    int displayBaselineCounter = 0;
    int displayBaseline = 0;
    bool displayBaselineLocked = false;

    // Hodnoty měřeného signálu
    int ambientBaseline = 0;
    int currentValue = 0;
    unsigned long lastPPGReadTime = 0;

    // Buffer pro vykreslení průběhu signálu
    int ppgBuffer[bufferSize] = {0};
    int bufferIndex = 0;

    // Hodnota EMA filtru
    float emaValue = 0.0f;

    // Detekce tepů a výpočet BPM
    unsigned long intervalBuffer[intervalBufferSize] = {0};
    int intervalIndex = 0;
    unsigned long lastPeakMillis = 0;
    bool signalAboveThreshold = false;
    int adjThreshold = 0;

    // Sdílené hodnoty pro displej a web
    int currentBPM = 0;
    int lastAcceptedBPM = 0;
    bool bpmIsValid = false;

    // Indikace detekovaného tepu LED diodou
    bool ledOn = false;
    unsigned long lastPeakLedTime = 0;
    static constexpr unsigned long bpmBlinkTime = 50;

    // Rozměry displeje
    static constexpr int DISPLAY_WIDTH = 128;
    static constexpr int DISPLAY_HEIGHT = 64;

    // Položky menu
    static constexpr int menuItemCount = 4;
    const char* menuItems[menuItemCount] = {
        "Kalibrace",
        "Autozoom",
        "Zpet",
        "Konec"
    };

    // Reset měření
    void resetMeasurementState();

    // Snímání a zpracování PPG signálu
    void sampleAndProcessPPG();
    int readPPG();

    // Detekce tepu a výpočet BPM
    bool detectPeak(int value, int threshold, unsigned long now);
    void calculateBPM(unsigned long interval);
    bool isBpmPlausible(int bpm) const;
    int calculateDynamicThreshold() const;

    // Ovládání stavu modulu
    void handleStateSwitch();
    void handleLedSwitch();

    // Vykreslení na displej
    void drawMeasurementScreen(bool fingerOn);
    void drawPPGCurve(Adafruit_SH1106G& display);
    void displayBPM(Adafruit_SH1106G& display, bool fingerOn);
    void drawMenu();

    // Indikace tepu LED diodou
    void blinkPeak(bool peak);

    // Uzamčení sdílených dat
    void lockData();
    void unlockData();
};
