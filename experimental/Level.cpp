#include <Level.h>

Level::Level(Controls& ctrl) 
    : controls(ctrl), currentState(LOOP), offset{0,0,0}, gyroRate{0,0,0} 
{}

void Level::start() {
    mpu.initialize();
    calibration();  // automatická kalibrace při startu
}

void Level::begin() {
    _exit = false;
    currentState = LOOP;
}

void Level::loop() {
    if (mpu.testConnection()) {
        switch(currentState) {
            case LOOP: {
                gyro accel = readAccel();
                gyro g     = readGyro();

                const float alpha = 0.98f;
                gyroRate.x = alpha*(gyroRate.x + g.x*0.01f) + (1-alpha)*(accel.x - offset.x);
                gyroRate.y = alpha*(gyroRate.y + g.y*0.01f) + (1-alpha)*(accel.y - offset.y);

                drawBall(gyroRate);

                if(controls.FingerTouchedFlag()) currentState = MENU;
                break;
            }
            case MENU:        
                menu(); 
                break;
            case CALIBRATION: 
                calibration(); 
                currentState = LOOP; 
                break;
        }
    } else {
        controls.display.clearDisplay();
        controls.display.setCursor(10, 10);
        controls.display.println("Gyroskop neni");
        controls.display.setCursor(25, 25);
        controls.display.println("pripojen!");
        controls.display.setCursor(10, 40);
        controls.display.println("Touch pro navrat!");
        controls.display.display();
        if (controls.FingerTouchedFlag()){
            _exit = true;
        }
    }
}

void Level::menu() {
    static int menuIndex = 0;             
    const int menuItems = 3;
    const char* menuStrings[menuItems] = {"Kalibrace", "Zpet", "Konec"};

    controls.display.clearDisplay();
    controls.display.setCursor(0,0);

    for(int itemIndex = 0; itemIndex < menuItems; itemIndex++){
        if(itemIndex == menuIndex) controls.display.print("> ");
        else controls.display.print("  ");

        controls.display.println(menuStrings[itemIndex]);
    }

    controls.display.display();

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
                calibration(); // kalibrace s progress barem
                currentState = LOOP; 
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

// Kalibrace s progress barem
void Level::calibration() {

    gyro sumAccel = {0,0,0};
    gyro sumGyro  = {0,0,0};
    const int samples = 100;

    if (mpu.testConnection()){

        controls.display.clearDisplay();
        controls.display.setTextSize(1);
        controls.display.setTextColor(SH110X_WHITE);
        controls.display.setCursor(40, 5);
        controls.display.println("GYROSKOP");

        controls.display.setCursor(0, 20);
        controls.display.println("Kalibrace senzoru...");
        controls.display.display();

        for(int i = 0; i < samples; i++) {

            gyro a = readAccel();
            gyro g = readGyro();

            sumAccel.x += a.x;
            sumAccel.y += a.y;
            sumAccel.z += a.z;

            sumGyro.x += g.x;
            sumGyro.y += g.y;
            sumGyro.z += g.z;

            // progress bar
            int progressWidth = map(i, 0, samples - 1, 0, 100);
            controls.display.fillRect(14, 35, progressWidth, 10, SH110X_WHITE);
            controls.display.display();

            delay(20);
        }

            offset.x = sumAccel.x / samples;
            offset.y = sumAccel.y / samples;
            offset.z = sumAccel.z / samples;

            gyroRate.x = sumGyro.x / samples;
            gyroRate.y = sumGyro.y / samples;
            gyroRate.z = sumGyro.z / samples;

            controls.display.clearDisplay();
            controls.display.setCursor(0, 20);
            controls.display.println("Kalibrace dokoncena!");
            controls.display.display();

            delay(1000);
    } else {
        controls.display.clearDisplay();
        controls.display.setCursor(10, 10);
        controls.display.println("Gyroskop neni");
        controls.display.setCursor(25, 25);
        controls.display.println("pripojen!");
        controls.display.display();
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
    int px = map(v.x, -17000, 17000, 0, controls.display.width()-1);
    int py = map(v.y, -17000, 17000, 0, controls.display.height()-1);

    controls.display.clearDisplay();
    controls.display.fillCircle(px, py, 3, SH110X_WHITE);
    controls.display.display();
}

bool Level::shouldExit() {
    return _exit;
}

void Level::web_Level() {
    web.server.on("/level/exit", HTTP_GET, [this](AsyncWebServerRequest *request) { // Koncový bod pro opuštění Level a návrat do hlavního menu přes HTTP požadavky
        _exit = true; // Nastavení indikátoru pro opuštění menu
        request->send(200,"text/plain","ok"); // Odeslání odpovědi pro potvrzení opuštění menu a návratu do hlavního menu
    });
}
