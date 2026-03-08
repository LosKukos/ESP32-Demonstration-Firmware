#pragma once
#include <MPU6050.h>
#include "controls.h"

class Level {
public:
    Level(Controls& ctrl);

    void start();       // inicializace + kalibrace při startu
    void begin();       // vstup do hlavní smyčky
    void loop();        // hlavní loop
    void menu();        // menu s volbou kalibrace
    void calibration(); // čistá kalibrace bez progress baru
    bool shouldExit();    // kontrola, zda uživatel chce opustit aplikaci

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
