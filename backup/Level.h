#ifndef LEVEL_H
#define LEVEL_H

#pragma once
#include <MPU6050.h>
#include <controls.h>
#include <web.h>

class Level {
public:
    Level(Controls& ctrl);

    void start();       // inicializace + kalibrace při startu
    void begin();       // vstup do hlavní smyčky
    void loop();        // hlavní loop
    void menu();        // menu s volbou kalibrace
    void calibration(); // kalibrace
    bool shouldExit();    // kontrola, zda uživatel chce opustit aplikaci
    void web_Level();

private:
    Controls& controls;
    MPU6050 mpu;

    struct gyro { float x, y, z; };
    gyro offset;     // offset z kalibrace
    gyro gyroRate;   // aktuální hodnota gyro+accel kombinace
    bool _exit = false;

    enum LevelState { LOOP, MENU, CALIBRATION } currentState;

    gyro readAccel();
    gyro readGyro();
    void drawBall(gyro v);
};

#endif