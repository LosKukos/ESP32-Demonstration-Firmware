#include <Level.h>
#include <math.h>

namespace {
    // Statická paměť pro FreeRTOS task modulu
    StaticTask_t levelTaskBuffer;
    StackType_t levelTaskStack[Level::taskStackSize];
}

// Položky menu v modulu
const char* const Level::menuItems[Level::menuItemCount] = {  
    "Kalibrace",
    "Zpet",
    "Konec"
};

Level::Level(Controls& ctrl, Settings& settingsRef) 
    : controls(ctrl), settings(settingsRef) {
    // Mutex pro sdílená data
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
    // Kontrola odpovědi senzoru MPU6050
    sensorReady = mpu.testConnection();
    return sensorReady;
}

void Level::showSensorError() {
    controls.mutexDisplay([&]() {
        auto& display = controls.display;

        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);

        display.setCursor((SCREEN_WIDTH - 8 * 5) / 2, 0);
        display.println("LEVEL");

        display.setCursor(10, 20);
        display.println("Chyba senzoru");

        display.setCursor(10, 32);
        display.println("Senzor neni");

        display.setCursor(10, 44);
        display.println("pripojen");

        display.display();

        vTaskDelay(pdMS_TO_TICKS(1000));
    });
}

void Level::waitForTouchExit() {
    controls.resetTouchState();

    // Čekání na uvolnění dotykového senzoru
    while (controls.fingerTouched()) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    controls.resetTouchState();

    // Čekání na potvrzení návratu dotykem
    while (!controls.FingerTouchedFlag()) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    controls.resetTouchState();
}

void Level::begin() {
    // Inicializace senzoru MPU a reset proměnných modulu
    mpu.initialize();

    _exit = false;
    running = true;
    currentState = LOOP;
    menuIndex = 0;
    calibrating = false;
    calibIndex = 0;

    // Kontrola připojení senzoru
    if (!checkSensorConnection()) {
        showSensorError();
        waitForTouchExit();
        running = false;
        _exit = true;
        return;
    }

    // Vytvoření nebo probuzení tasku modulu
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
            return;
        }

    } else {
        vTaskResume(taskHandle);
    }
}

void Level::task(void *pvParameters) {
    Level* self = static_cast<Level*>(pvParameters);

    for (;;) {
        // Pokud modul neběží, task se uspí
        if (!self->running) { 
            vTaskSuspend(nullptr);
        }

        // Ukončení modulu a nastavení výchozího stavu
        if (self->_exit) {
            self->calibrating = false;
            self->menuIndex = 0;
            self->currentState = LOOP;
            self->running = false;
            self->setLevelLed(0, 0, 0);
            vTaskSuspend(nullptr);
            continue;
        }

        // Stavový automat modulu
        switch (self->currentState) {
            case LOOP:
                // Výpočet náklonu a vykreslení pozice kuličky
                self->updateTiltEstimate();

                // Kontrola požadavku na ukončení
                if (self->_exit) {
                    continue;
                }

                self->drawBall(self->tiltEstimate);

                // Dotyk otevře menu
                if (self->controls.FingerTouchedFlag()) {
                    self->currentState = MENU;
                }
                break;

            case MENU:
                self->setLevelLed(0, 0, 0);
                self->drawMenu();
                break;

            case CALIBRATION:
                self->setLevelLed(0, 0, 0);

                // Spuštění kalibrace při vstupu do stavu
                if (!self->calibrating) {
                    self->startCalibration();
                }

                self->calibrationStep();

                // Návrat do měření po dokončení kalibrace
                if (!self->calibrating && !self->_exit) {
                    self->currentState = LOOP;
                }
                break;
        }

        vTaskDelay(taskDelayTicks);
    }
}

void Level::drawMenu() {
    controls.mutexDisplay([&]() {
        auto& display = controls.display;

        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);
        display.setCursor((SCREEN_WIDTH - 8 * 5) / 2, 0);
        display.println("LEVEL");

        for (int itemIndex = 0; itemIndex < menuItemCount; itemIndex++) {
            display.setCursor(10, 20 + itemIndex * 12);
            display.print(itemIndex == menuIndex ? "> " : "  ");
            display.println(menuItems[itemIndex]);
        }

        display.display();
    });

    // Zpracování vstupů pro navigaci v menu
    if (controls.leftPressed) {
        menuIndex = (menuIndex - 1 + menuItemCount) % menuItemCount;
        controls.leftPressed = false;
    }

    if (controls.rightPressed) {
        menuIndex = (menuIndex + 1) % menuItemCount;
        controls.rightPressed = false;
    }

    // Výběr položky pomocí dotyku
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
    // Načtení dat ze senzoru
    Axis accel = readAccel(); 
    Axis gyro = readGyro(); 

    // Odečtení kalibračních hodnot od naměřených dat
    const float accelX = accel.x - accelOffset.x; 
    const float accelY = accel.y - accelOffset.y;
    const float gyroX = gyro.x - gyroBias.x;
    const float gyroY = gyro.y - gyroBias.y;

    // Komplementární filtr
    // Gyroskop zpracovává okamžitou změnu, akcelerometr slouží ke korekci
    Axis newTilt;
    newTilt.x = filterGyroWeight * (tiltEstimate.x + gyroX * integrationStepSeconds)
              + filterAccelWeight * accelX;

    newTilt.y = filterGyroWeight * (tiltEstimate.y + gyroY * integrationStepSeconds)
              + filterAccelWeight * accelY;

    newTilt.z = accel.z - accelOffset.z;

    // Kontrola neplatných hodnot výpočtu
    if (!isfinite(newTilt.x) || !isfinite(newTilt.y)) {
        if (!checkSensorConnection()) {
            showSensorError();
            _exit = true;
            return;
        }

        newTilt = {0.0f, 0.0f, 0.0f};
    }

    // Uložení vypočtených hodnot
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

    // Vykreslení os na displeji
    for (int y = 0; y < display.height(); y += 4) {
        display.drawPixel(cx, y, SH110X_WHITE);
    }

    for (int x = 0; x < display.width(); x += 4) {
        display.drawPixel(x, cy, SH110X_WHITE);
    }
}

void Level::updateCenterLedFromPixel(int px, int py) {
    auto& display = controls.display;

    // Výpočet vzdálenosti kuličky od středu displeje
    const float cx = display.width() * 0.5f;
    const float cy = display.height() * 0.5f;

    const float dx = static_cast<float>(px) - cx;
    const float dy = static_cast<float>(py) - cy;
    const float dist = sqrtf(dx * dx + dy * dy);

    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;

    // Kulička mimo středovou oblast
    if (dist >= ledOuterRadiusPx) {
        red = 0;
        green = 0;
        blue = 0;
    }
    // Přiblížení ke středu je signalizováno červeně
    else if (dist > ledInnerRadiusPx) {
        float t = (ledOuterRadiusPx - dist) / (ledOuterRadiusPx - ledInnerRadiusPx);
        t = constrain(t, 0.0f, 1.0f);
        t = t * t;

        red = static_cast<uint8_t>(255.0f * t);
        green = 0;
        blue = 0;
    }
    // Vycentrovaná poloha je signalizována zeleně
    else {
        float t = dist / ledInnerRadiusPx;
        t = constrain(t, 0.0f, 1.0f);
        t = t * t * t;

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

        // Mapování vypočtených hodnot na pozici kuličky
        long mappedX = map(static_cast<long>(tilt.x), -displayInputRange, displayInputRange, 0, display.width() - 1);
        long mappedY = map(static_cast<long>(tilt.y), -displayInputRange, displayInputRange, 0, display.height() - 1);

        // Omezení pozice kuličky na plochu displeje
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

void Level::registerWebRoutes() {
    if (webRoutesRegistered) {
        return;
    }
    webRoutesRegistered = true;

    // Vrácení dat modulu pro webové rozhraní
    settings.server().on("/level/update", HTTP_GET, [this](AsyncWebServerRequest *request) {
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

    // Vyvolání kalibrace z webu
    settings.server().on("/level/calibrate", HTTP_GET, [this](AsyncWebServerRequest *request) {
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

    // Ukončení modulu z webu
    settings.server().on("/level/exit", HTTP_GET, [this](AsyncWebServerRequest *request) {
        _exit = true;
        request->send(200, "text/plain", "ok");
    });
}

void Level::startCalibration() {
    // Nulování pomocných proměnných
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

    // Obrazovka kalibrace
    controls.mutexDisplay([&]() {
        auto& display = controls.display;

        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);

        display.setCursor((SCREEN_WIDTH - 8 * 5) / 2, 0);
        display.println("LEVEL");

        display.setCursor((SCREEN_WIDTH - 9 * 6) / 2, 20);
        display.println("Kalibrace");

        display.setCursor((SCREEN_WIDTH - 13 * 6) / 2, 32);
        display.println("Polozte desku");

        display.drawRect(calibrationBarX - 1, calibrationBarY - 1,
                         calibrationBarWidth + 2, calibrationBarHeight + 2,
                         SH110X_WHITE);

        display.display();
    });
}

void Level::calibrationStep() {
    // Kontrola senzoru během kalibrace
    if (!checkSensorConnection()) {
        showSensorError();
        calibrating = false;
        _exit = true;
        return;
    }

    // Sběr vzorků pro výpočet kalibračních hodnot
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

    // Výpočet kalibračních hodnot jako průměru ze vzorků
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

    // Zobrazení dokončení kalibrace
    controls.mutexDisplay([&]() {
        auto& display = controls.display;

        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);

        display.setCursor((SCREEN_WIDTH - 8 * 5) / 2, 0);
        display.println("LEVEL");

        display.setCursor((SCREEN_WIDTH - 9 * 6) / 2, 24);
        display.println("Kalibrace");

        display.setCursor((SCREEN_WIDTH - 10 * 6) / 2, 36);
        display.println("dokoncena");

        display.display();
    });
}

void Level::calibrate() {
    // Jednorázová kalibrace mimo task
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
