#pragma once

#include <Arduino.h>
#include <MPU6050.h>
#include <ArduinoJson.h>
#include <controls.h>
#include <settings.h>

class Level {
public:
    Level(Controls& ctrl, Settings& settingsRef);

    // Inicializace modulu
    void begin();

    // Stav modulu
    bool shouldExit();
    bool isRunning();
    void requestStop() { _exit = true; }

    // Webové rozhraní
    void registerWebRoutes();

    // Kalibrace senzoru
    void calibrate();

    static constexpr uint16_t taskStackSize = 2048;

private:
    struct Axis {
        float x;
        float y;
        float z;
    };

    enum LevelState {
        LOOP,
        MENU,
        CALIBRATION
    };

    // Nastavení tasku
    static constexpr BaseType_t taskPriority = 1;
    static constexpr BaseType_t taskCore = 1;
    static constexpr TickType_t taskDelayTicks = pdMS_TO_TICKS(20);
    static constexpr TickType_t calibrationStepDelayTicks = pdMS_TO_TICKS(20);
    bool running = false;
    bool webRoutesRegistered = false;

    // Nastavení komplementárního filtru
    static constexpr float filterGyroWeight = 0.98f;
    static constexpr float filterAccelWeight = 1.0f - filterGyroWeight;
    static constexpr float integrationStepSeconds = 0.02f;

    // Nastavení kalibrace
    static constexpr int calibrationSampleCount = 100;
    static constexpr int calibrationBarX = 14;
    static constexpr int calibrationBarY = 47;
    static constexpr int calibrationBarWidth = 100;
    static constexpr int calibrationBarHeight = 10;

    // Nastavení displeje
    static constexpr int ballRadius = 3;
    static constexpr long displayInputRange = 17000;
    static constexpr int menuItemCount = 3;
    static const char* const menuItems[menuItemCount];

    // Nastavení LED diody
    static constexpr int ledInnerRadiusPx = 8;
    static constexpr int ledOuterRadiusPx = 20;

    Controls& controls;
    Settings& settings;
    MPU6050 mpu;

    // FreeRTOS task modulu
    static void task(void *pvParameters);
    TaskHandle_t taskHandle = nullptr;
    SemaphoreHandle_t dataMutex = nullptr;

    LevelState currentState = LOOP;
    volatile bool _exit = false;
    bool sensorReady = false;

    // Kalibrační a vypočtené hodnoty senzoru
    Axis accelOffset{0.0f, 0.0f, 0.0f};
    Axis gyroBias{0.0f, 0.0f, 0.0f};
    Axis tiltEstimate{0.0f, 0.0f, 0.0f};
    Axis accelSum{0.0f, 0.0f, 0.0f};
    Axis gyroSum{0.0f, 0.0f, 0.0f};

    // Stav kalibrace a menu
    bool calibrating = false;
    int calibIndex = 0;
    int menuIndex = 0;

    // Práce se senzorem
    bool checkSensorConnection();
    Axis readAccel();
    Axis readGyro();
    void updateTiltEstimate();

    // Kalibrace senzoru
    void startCalibration();
    void calibrationStep();

    // Vykreslování na displej
    void drawMenu();
    void drawBall(const Axis& tilt);
    void drawAxesAndZones();
    void showSensorError();
    void waitForTouchExit();

    // Ovládání LED diody
    void updateCenterLedFromPixel(int px, int py);
    void setLevelLed(uint8_t red, uint8_t green, uint8_t blue);

    // Uzamčení sdílených dat
    bool lockData(TickType_t ticks = portMAX_DELAY);
    void unlockData();
};
