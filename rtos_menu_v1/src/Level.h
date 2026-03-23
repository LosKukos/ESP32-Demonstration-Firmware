#ifndef LEVEL_H
#define LEVEL_H

#pragma once

#include <MPU6050.h>
#include <controls.h>
#include <web.h>
#include <Arduino.h>

class Level {
public:
    Level(Controls& ctrl);

    void start();        // inicializace + start tasku
    void menu();         // menu s volbou kalibrace
    bool shouldExit();   // kontrola ukončení
    void web_Level();
    void gyro_calib();
    bool isRunning();

private:
    Controls& controls;
    MPU6050 mpu;

    // Task
    static void task(void *pvParameters);
    TaskHandle_t taskHandle;

    enum LevelState { LOOP, MENU, CALIBRATION } currentState;

    struct gyro { float x, y, z; };

    gyro offset;     
    gyro gyroRate;   

    volatile bool _exit = false;

    // Calibration
    bool calibrating = false;
    int calibIndex = 0;

    gyro sumAccel;
    gyro sumGyro;

    int menuIndex;

    // Interní funkce
    void startCalibration();
    void calibrationStep();

    gyro readAccel();
    gyro readGyro();
    void drawBall(gyro v);
};

#endif