#ifndef LEVEL_H
#define LEVEL_H

#pragma once

#include <Arduino.h>
#include <MPU6050.h>
#include <ArduinoJson.h>
#include <controls.h>
#include <web.h>

class Level {
public:
    Level(Controls& ctrl);

    void start();
    void menu();
    bool shouldExit();
    void web_Level();
    void gyro_calib();
    bool isRunning();

    TaskHandle_t getTaskHandle() const { return taskHandle; }
    void clearTaskHandle() { taskHandle = nullptr; }
    void requestStop() { _exit = true; }

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

    // ===== Runtime =====
    static constexpr BaseType_t taskPriority = 1;
    static constexpr BaseType_t taskCore = 1;
    static constexpr TickType_t taskDelayTicks = pdMS_TO_TICKS(20);
    static constexpr TickType_t calibrationStepDelayTicks = pdMS_TO_TICKS(20);
    bool running = false;
    bool webRoutesRegistered = false;

    // ===== Complementary filter =====
    static constexpr float filterGyroWeight = 0.98f;
    static constexpr float filterAccelWeight = 1.0f - filterGyroWeight;
    static constexpr float integrationStepSeconds = 0.02f;

    // ===== Calibration =====
    static constexpr int calibrationSampleCount = 100;
    static constexpr int calibrationBarX = 14;
    static constexpr int calibrationBarY = 35;
    static constexpr int calibrationBarWidth = 100;
    static constexpr int calibrationBarHeight = 10;

    // ===== Display =====
    static constexpr int ballRadius = 3;
    static constexpr long displayInputRange = 17000;
    static constexpr int menuItemCount = 3;
    static const char* const menuItems[menuItemCount];

    // ===== LED / center guidance =====
    static constexpr int ledInnerRadiusPx = 8;
    static constexpr int ledOuterRadiusPx = 20;

    Controls& controls;
    MPU6050 mpu;

    static void task(void *pvParameters);
    TaskHandle_t taskHandle = nullptr;
    SemaphoreHandle_t dataMutex = nullptr;

    LevelState currentState = LOOP;
    volatile bool _exit = false;
    bool sensorReady = false;

    Axis accelOffset{0.0f, 0.0f, 0.0f};
    Axis gyroBias{0.0f, 0.0f, 0.0f};
    Axis tiltEstimate{0.0f, 0.0f, 0.0f};
    Axis accelSum{0.0f, 0.0f, 0.0f};
    Axis gyroSum{0.0f, 0.0f, 0.0f};

    bool calibrating = false;
    int calibIndex = 0;
    int menuIndex = 0;

    bool checkSensorConnection();
    void showSensorError();
    void startCalibration();
    void calibrationStep();
    void updateTiltEstimate();

    Axis readAccel();
    Axis readGyro();
    void drawBall(const Axis& tilt);

    void drawAxesAndZones();
    void updateCenterLedFromPixel(int px, int py);
    void setLevelLed(uint8_t red, uint8_t green, uint8_t blue);

    bool lockData(TickType_t ticks = portMAX_DELAY);
    void unlockData();
};

#endif