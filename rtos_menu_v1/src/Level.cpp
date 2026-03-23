#include <Level.h>

Level::Level(Controls& ctrl)
    : controls(ctrl),
      currentState(LOOP),
      menuIndex(0),
      offset{0,0,0},
      gyroRate{0,0,0} {}

void Level::start() {
    if (taskHandle != NULL) return;
    mpu.initialize();
    _exit = false;

    xTaskCreatePinnedToCore(
        task,
        "LevelTask",
        4096,
        this,
        1,
        &taskHandle,
        1
    );
}

void Level::task(void *pvParameters) {
    Level* self = static_cast<Level*>(pvParameters);

    for (;;) {

        if (self->_exit) {
            break;
        }

        if (self->mpu.testConnection()) {

            switch (self->currentState) {

                case LOOP: {
                    gyro accel = self->readAccel();
                    gyro g     = self->readGyro();

                    const float alpha = 0.98f;

                    self->gyroRate.x = alpha * (self->gyroRate.x + g.x * 0.01f)
                                     + (1 - alpha) * (accel.x - self->offset.x);

                    self->gyroRate.y = alpha * (self->gyroRate.y + g.y * 0.01f)
                                     + (1 - alpha) * (accel.y - self->offset.y);

                    self->drawBall(self->gyroRate);

                    if (self->controls.FingerTouchedFlag()) {
                        self->currentState = MENU;
                    }
                    break;
                }

                case MENU:
                    self->menu();
                    break;

                case CALIBRATION: {

                    if (!self->calibrating) {
                        self->startCalibration();
                    }

                    self->calibrationStep();

                    if (!self->calibrating) {
                        self->currentState = LOOP;
                    }

                    break;
                }
            }

        } else {

            self->controls.mutexDisplay([&]() {
                auto& display = self->controls.display;

                display.clearDisplay();
                display.setCursor(10, 10);
                display.println("Gyroskop neni");

                display.setCursor(25, 25);
                display.println("pripojen!");

                display.setCursor(10, 40);
                display.println("Touch pro navrat!");

                display.display();
            });

            if (self->controls.FingerTouchedFlag()) {
                self->_exit = true;
            }
        }

        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
    self->taskHandle = NULL;
    vTaskDelete(NULL);
}

void Level::menu() {          
    const int menuItems = 3;
    const char* menuStrings[menuItems] = {"Kalibrace", "Zpet", "Konec"};

    controls.mutexDisplay([&]() {

        auto& display = controls.display;

        display.clearDisplay();
        display.setCursor(0,0);

        for(int itemIndex = 0; itemIndex < menuItems; itemIndex++){
            if(itemIndex == menuIndex) display.print("> ");
            else display.print("  ");

            display.println(menuStrings[itemIndex]);
        }

        display.display();
    });

    if(controls.leftPressed){
        menuIndex = (menuIndex - 1 + menuItems) % menuItems;
        controls.leftPressed = false;
    }

    if(controls.rightPressed){
        menuIndex = (menuIndex + 1) % menuItems;
        controls.rightPressed = false;
    }

    if(controls.FingerTouchedFlag()){
        switch(menuIndex){
            case 0: 
                startCalibration();
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

Level::gyro Level::readAccel() {
    int16_t ax, ay, az;
    mpu.getAcceleration(&ax, &ay, &az);
    return {(float)ax, (float)ay, (float)az};
}

Level::gyro Level::readGyro() {
    int16_t gx, gy, gz;
    mpu.getRotation(&gx, &gy, &gz);
    return {(float)gx, (float)gy, (float)gz};
}

void Level::drawBall(gyro v) {

    controls.mutexDisplay([&]() {

        auto& display = controls.display;

        int px = map(v.x, -17000, 17000, 0, display.width() - 1);
        int py = map(v.y, -17000, 17000, 0, display.height() - 1);

        display.clearDisplay();
        display.fillCircle(px, py, 3, SH110X_WHITE);
        display.display();

    });
}

bool Level::shouldExit() {
    return _exit;
}

void Level::web_Level() {

    web.server().on("/level/exit", HTTP_GET, [this](AsyncWebServerRequest *request) {
        _exit = true;
        request->send(200, "text/plain", "ok");
    });

}

void Level::startCalibration() {

    calibrating = true;
    calibIndex = 0;

    sumAccel = {0,0,0};
    sumGyro  = {0,0,0};

    controls.mutexDisplay([&]() {

        auto& display = controls.display;

        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(40, 5);
        display.println("GYROSKOP");

        display.setCursor(0, 20);
        display.println("Kalibrace senzoru...");
        display.display();

    });
}

void Level::calibrationStep() {

    const int samples = 100;

    if (!mpu.testConnection()) {
        controls.mutexDisplay([&]() {
            auto& display = controls.display;

            display.clearDisplay();
            display.setCursor(10, 10);
            display.println("Gyroskop neni");
            display.setCursor(25, 25);
            display.println("pripojen!");
            display.display();
        });

        calibrating = false;
        _exit = true;
        return;
    }

    if (calibIndex < samples) {

        gyro a = readAccel();
        gyro g = readGyro();

        sumAccel.x += a.x;
        sumAccel.y += a.y;
        sumAccel.z += a.z;

        sumGyro.x += g.x;
        sumGyro.y += g.y;
        sumGyro.z += g.z;

        int progressWidth = map(calibIndex, 0, samples - 1, 0, 100);

        controls.mutexDisplay([&]() {
            auto& display = controls.display;

            display.fillRect(14, 35, progressWidth, 10, SH110X_WHITE);
            display.display();
        });

        calibIndex++;

    } else {

        offset.x = sumAccel.x / samples;
        offset.y = sumAccel.y / samples;
        offset.z = sumAccel.z / samples;

        gyroRate.x = sumGyro.x / samples;
        gyroRate.y = sumGyro.y / samples;
        gyroRate.z = sumGyro.z / samples;

        controls.mutexDisplay([&]() {
            auto& display = controls.display;

            display.clearDisplay();
            display.setCursor(0, 20);
            display.println("Kalibrace dokoncena!");
            display.display();
        });

        calibrating = false;
    }
}

void Level::gyro_calib(){
    startCalibration();

    while (calibrating) {
        calibrationStep();
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

bool Level::isRunning() {
    return taskHandle != NULL;
}