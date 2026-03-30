#include <Level.h>
#include <math.h>

namespace {
StaticTask_t levelTaskBuffer;
StackType_t levelTaskStack[Level::taskStackSize];
}

const char* const Level::menuItems[Level::menuItemCount] = {
    "Kalibrace",
    "Zpet",
    "Konec"
};

Level::Level(Controls& ctrl)
    : controls(ctrl) {
    dataMutex = xSemaphoreCreateMutex();
}

bool Level::lockData(TickType_t ticks) {
    return dataMutex != nullptr && xSemaphoreTake(dataMutex, ticks) == pdTRUE;
}

void Level::unlockData() {
    if (dataMutex != nullptr) {
        xSemaphoreGive(dataMutex);
    }
}

bool Level::checkSensorConnection() {
    sensorReady = mpu.testConnection();
    return sensorReady;
}

void Level::showSensorError() {
    controls.mutexDisplay([&]() {
        auto& display = controls.display;

        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(10, 10);
        display.println("MPU6050 neni");
        display.setCursor(25, 25);
        display.println("pripojen!");
        display.setCursor(10, 40);
        display.println("Touch pro navrat");
        display.display();
    });
}

void Level::start() {
    mpu.initialize();

    _exit = false;
    running = true;
    currentState = LOOP;
    menuIndex = 0;
    calibrating = false;
    calibIndex = 0;

    if (!checkSensorConnection()) {
        showSensorError();
        running = false;
        _exit = true;
        return;
    }

    if (taskHandle == nullptr) {
        taskHandle = xTaskCreateStaticPinnedToCore(
            task,
            "LevelTask",
            taskStackSize,
            this,
            taskPriority,
            levelTaskStack,
            &levelTaskBuffer,
            taskCore
        );

        if (taskHandle == nullptr) {
            running = false;
            Serial.println("[LEVEL] static task create FAIL");
            return;
        }

        Serial.println("[LEVEL] static task create OK");
    } else {
        vTaskResume(taskHandle);
        Serial.println("[LEVEL] task resumed");
    }
}

void Level::task(void *pvParameters) {
    Level* self = static_cast<Level*>(pvParameters);

    for (;;) {
        if (!self->running) {
            vTaskSuspend(nullptr);
        }

        if (self->_exit) {
            self->calibrating = false;
            self->menuIndex = 0;
            self->currentState = LOOP;
            self->running = false;

            self->setLevelLed(0, 0, 0);

            UBaseType_t highWaterWords = uxTaskGetStackHighWaterMark(nullptr);
            Serial.printf("[LEVEL] task suspended | stack free minimum: %u bytes\n",
                          (unsigned int)(highWaterWords * sizeof(StackType_t)));

            vTaskSuspend(nullptr);
            continue;
        }

        switch (self->currentState) {
            case LOOP:
                self->updateTiltEstimate();
                if (self->_exit) {
                    continue;
                }

                self->drawBall(self->tiltEstimate);

                if (self->controls.FingerTouchedFlag()) {
                    self->currentState = MENU;
                }
                break;

            case MENU:
                self->setLevelLed(0, 0, 0);
                self->menu();
                break;

            case CALIBRATION:
                self->setLevelLed(0, 0, 0);

                if (!self->calibrating) {
                    self->startCalibration();
                }

                self->calibrationStep();

                if (!self->calibrating && !self->_exit) {
                    self->currentState = LOOP;
                }
                break;
        }

        vTaskDelay(taskDelayTicks);
    }
}

void Level::menu() {
    controls.mutexDisplay([&]() {
        auto& display = controls.display;

        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(0, 0);
        display.println("LEVEL MENU");

        for (int itemIndex = 0; itemIndex < menuItemCount; itemIndex++) {
            display.setCursor(0, 14 + itemIndex * 12);
            display.print(itemIndex == menuIndex ? "> " : "  ");
            display.println(menuItems[itemIndex]);
        }

        display.display();
    });

    if (controls.leftPressed) {
        menuIndex = (menuIndex - 1 + menuItemCount) % menuItemCount;
        controls.leftPressed = false;
    }

    if (controls.rightPressed) {
        menuIndex = (menuIndex + 1) % menuItemCount;
        controls.rightPressed = false;
    }

    if (controls.FingerTouchedFlag()) {
        switch (menuIndex) {
            case 0:
                currentState = CALIBRATION;
                break;

            case 1:
                currentState = LOOP;
                break;

            case 2:
                _exit = true;
                break;
        }
    }
}

void Level::updateTiltEstimate() {
    Axis accel = readAccel();
    Axis gyro = readGyro();

    const float accelX = accel.x - accelOffset.x;
    const float accelY = accel.y - accelOffset.y;
    const float gyroX = gyro.x - gyroBias.x;
    const float gyroY = gyro.y - gyroBias.y;

    Axis newTilt;
    newTilt.x = filterGyroWeight * (tiltEstimate.x + gyroX * integrationStepSeconds)
              + filterAccelWeight * accelX;

    newTilt.y = filterGyroWeight * (tiltEstimate.y + gyroY * integrationStepSeconds)
              + filterAccelWeight * accelY;

    newTilt.z = accel.z - accelOffset.z;

    if (!isfinite(newTilt.x) || !isfinite(newTilt.y)) {
        if (!checkSensorConnection()) {
            showSensorError();
            _exit = true;
            return;
        }

        newTilt = {0.0f, 0.0f, 0.0f};
    }

    if (lockData(pdMS_TO_TICKS(10))) {
        tiltEstimate = newTilt;
        unlockData();
    } else {
        tiltEstimate = newTilt;
    }
}

Level::Axis Level::readAccel() {
    int16_t ax, ay, az;
    mpu.getAcceleration(&ax, &ay, &az);
    return {static_cast<float>(ax), static_cast<float>(ay), static_cast<float>(az)};
}

Level::Axis Level::readGyro() {
    int16_t gx, gy, gz;
    mpu.getRotation(&gx, &gy, &gz);
    return {static_cast<float>(gx), static_cast<float>(gy), static_cast<float>(gz)};
}

void Level::drawAxesAndZones() {
    auto& display = controls.display;

    const int cx = display.width() / 2;
    const int cy = display.height() / 2;

    for (int y = 0; y < display.height(); y += 4) {
        display.drawPixel(cx, y, SH110X_WHITE);
    }

    for (int x = 0; x < display.width(); x += 4) {
        display.drawPixel(x, cy, SH110X_WHITE);
    }
}

void Level::updateCenterLedFromPixel(int px, int py) {
    auto& display = controls.display;

    const float cx = display.width() * 0.5f;
    const float cy = display.height() * 0.5f;

    const float dx = static_cast<float>(px) - cx;
    const float dy = static_cast<float>(py) - cy;
    const float dist = sqrtf(dx * dx + dy * dy);

    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;

    if (dist >= ledOuterRadiusPx) {
        red = 0;
        green = 0;
        blue = 0;
    }
    else if (dist > ledInnerRadiusPx) {
        float t = (ledOuterRadiusPx - dist) / (ledOuterRadiusPx - ledInnerRadiusPx);
        t = constrain(t, 0.0f, 1.0f);
        t = t * t;  // jemnější fade-in z černé do červené

        red = static_cast<uint8_t>(255.0f * t);
        green = 0;
        blue = 0;
    }
    else {
        float t = dist / ledInnerRadiusPx;
        t = constrain(t, 0.0f, 1.0f);
        t = t * t * t;  // pomalejší přechod z červené do zelené

        red = static_cast<uint8_t>(255.0f * t);
        green = static_cast<uint8_t>(255.0f * (1.0f - t));
        blue = 0;
    }

    setLevelLed(red, green, blue);
}

void Level::setLevelLed(uint8_t red, uint8_t green, uint8_t blue) {
    controls.mutexLed([&]() {
        controls.strip.setPixelColor(0, controls.strip.Color(red, green, blue));
        controls.strip.show();
    });
}

void Level::drawBall(const Axis& tilt) {
    controls.mutexDisplay([&]() {
        auto& display = controls.display;

        long mappedX = map(static_cast<long>(tilt.x), -displayInputRange, displayInputRange, 0, display.width() - 1);
        long mappedY = map(static_cast<long>(tilt.y), -displayInputRange, displayInputRange, 0, display.height() - 1);

        const int px = constrain(static_cast<int>(mappedX), ballRadius, display.width() - 1 - ballRadius);
        const int py = constrain(static_cast<int>(mappedY), ballRadius, display.height() - 1 - ballRadius);

        display.clearDisplay();
        drawAxesAndZones();
        display.fillCircle(px, py, ballRadius, SH110X_WHITE);
        display.display();

        updateCenterLedFromPixel(px, py);
    });
}

bool Level::shouldExit() {
    return _exit;
}

void Level::web_Level() {
    if (webRoutesRegistered) {
        return;
    }
    webRoutesRegistered = true;

    web.server().on("/level/update", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (_exit || !running) {
            request->send(503, "application/json", "{}");
            return;
        }

        Axis tiltSnapshot = {0.0f, 0.0f, 0.0f};
        bool calibratingSnapshot = false;
        bool sensorSnapshot = false;

        if (lockData(pdMS_TO_TICKS(10))) {
            tiltSnapshot = tiltEstimate;
            calibratingSnapshot = calibrating;
            sensorSnapshot = sensorReady;
            unlockData();
        } else {
            tiltSnapshot = tiltEstimate;
            calibratingSnapshot = calibrating;
            sensorSnapshot = sensorReady;
        }

        JsonDocument doc;
        doc["x"] = tiltSnapshot.x;
        doc["y"] = tiltSnapshot.y;
        doc["z"] = tiltSnapshot.z;
        doc["calibrating"] = calibratingSnapshot;
        doc["sensorOk"] = sensorSnapshot;

        String json;
        serializeJson(doc, json);

        AsyncWebServerResponse* response = request->beginResponse(200, "application/json", json);
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Cache-Control", "no-cache");
        request->send(response);
    });

    web.server().on("/level/calibrate", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (_exit || !running) {
            request->send(503, "text/plain", "not running");
            return;
        }

        if (lockData(pdMS_TO_TICKS(10))) {
            calibrating = false;
            currentState = CALIBRATION;
            unlockData();
        } else {
            calibrating = false;
            currentState = CALIBRATION;
        }

        request->send(200, "text/plain", "ok");
    });

    web.server().on("/level/exit", HTTP_GET, [this](AsyncWebServerRequest *request) {
        _exit = true;
        request->send(200, "text/plain", "ok");
    });
}

void Level::startCalibration() {
    if (lockData(pdMS_TO_TICKS(10))) {
        calibrating = true;
        calibIndex = 0;
        accelSum = {0.0f, 0.0f, 0.0f};
        gyroSum = {0.0f, 0.0f, 0.0f};
        unlockData();
    } else {
        calibrating = true;
        calibIndex = 0;
        accelSum = {0.0f, 0.0f, 0.0f};
        gyroSum = {0.0f, 0.0f, 0.0f};
    }

    controls.mutexDisplay([&]() {
        auto& display = controls.display;

        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(34, 5);
        display.println("GYROSKOP");
        display.setCursor(0, 20);
        display.println("Kalibrace senzoru...");
        display.display();
    });
}

void Level::calibrationStep() {
    if (!checkSensorConnection()) {
        showSensorError();
        calibrating = false;
        _exit = true;
        return;
    }

    if (calibIndex < calibrationSampleCount) {
        const Axis accel = readAccel();
        const Axis gyro = readGyro();

        accelSum.x += accel.x;
        accelSum.y += accel.y;
        accelSum.z += accel.z;

        gyroSum.x += gyro.x;
        gyroSum.y += gyro.y;
        gyroSum.z += gyro.z;

        const int progressWidth = map(calibIndex, 0, calibrationSampleCount - 1, 0, calibrationBarWidth);

        controls.mutexDisplay([&]() {
            auto& display = controls.display;
            display.fillRect(calibrationBarX, calibrationBarY, progressWidth, calibrationBarHeight, SH110X_WHITE);
            display.display();
        });

        calibIndex++;
        vTaskDelay(calibrationStepDelayTicks);
        return;
    }

    accelOffset.x = accelSum.x / calibrationSampleCount;
    accelOffset.y = accelSum.y / calibrationSampleCount;
    accelOffset.z = accelSum.z / calibrationSampleCount;

    gyroBias.x = gyroSum.x / calibrationSampleCount;
    gyroBias.y = gyroSum.y / calibrationSampleCount;
    gyroBias.z = gyroSum.z / calibrationSampleCount;

    if (lockData(pdMS_TO_TICKS(10))) {
        tiltEstimate = {0.0f, 0.0f, 0.0f};
        calibrating = false;
        unlockData();
    } else {
        tiltEstimate = {0.0f, 0.0f, 0.0f};
        calibrating = false;
    }

    controls.mutexDisplay([&]() {
        auto& display = controls.display;

        display.clearDisplay();
        display.setCursor(0, 20);
        display.println("Kalibrace dokoncena!");
        display.display();
    });
}

void Level::gyro_calib() {
    if (!checkSensorConnection()) {
        showSensorError();
        _exit = true;
        return;
    }

    startCalibration();

    while (calibrating) {
        calibrationStep();
        if (_exit) {
            break;
        }
    }
}

bool Level::isRunning() {
    return running;
}