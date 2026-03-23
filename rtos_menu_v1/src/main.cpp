#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <Wire.h>
#include <WiFi.h>
#include <SnakeLib.h>
#include <rgbmenu.h>
#include <controls.h>
#include <settings.h>
#include <PPG.h>
#include <Level.h>
#include <web.h>

// ---------------- GLOBAL ----------------
Controls controls;
auto& display = controls.display;

unsigned long lastStateChange = 0;

// WiFi ready flag
volatile bool wifiReady = false;

// Menu
const int menuLength = 5;
const char* menuItems[menuLength] = { "PPG", "Gyroskop", "Had", "Ovladani LED", "Nastaveni" };
int selected = 0;

int firstVisible = 0;
const int itemHeight = 12;
const int topOffset = 20;
const int visibleItems = 4;

// Modules
SnakeLib snake(controls);
RGBMenu rgbMenu(controls);
Settings settings(controls);
PPG ppg(controls);
Level level(controls);

//debug
size_t freeHeap = ESP.getFreeHeap();
size_t totalHeap = 327680;

float heapPercent = (float)freeHeap / (float)totalHeap * 100.0f;

// ---------------- STATE ----------------
enum AppState { MENU, APP_PPG, LEVEL_APP, SNAKE, RGB, SETTINGS };

volatile AppState state = MENU;

SemaphoreHandle_t stateMutex;

// request mechanism
volatile AppState requestedState = MENU;
volatile bool stateChangeRequested = false;

// ---------------- STATE SAFE ACCESS ----------------
void setState(AppState newState) {
    if (xSemaphoreTake(stateMutex, portMAX_DELAY)) {
        state = newState;
        xSemaphoreGive(stateMutex);
    }
}

AppState getState() {
    AppState s;
    if (xSemaphoreTake(stateMutex, portMAX_DELAY)) {
        s = state;
        xSemaphoreGive(stateMutex);
    }
    return s;
}

// ---------------- WIFI EVENT ----------------
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {

    switch (event) {

        case ARDUINO_EVENT_WIFI_AP_START:
            wifiReady = true;
            Serial.println("WiFi AP is READY");
            break;

        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            Serial.println("Client connected to AP");
            break;

        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            Serial.println("Client disconnected from AP");
            break;

        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
            Serial.println("Client got IP");
            break;

        default:
            break;
    }
}

// ---------------- MENU DRAW ----------------
void drawMenu() {

    controls.mutexDisplay([&]() {

        auto& display = controls.display;

        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);

        display.setCursor((SCREEN_WIDTH - 8 * 11) / 2, 0);
        display.println("HLAVNI MENU");

        for (int i = 0; i < visibleItems; i++) {
            int itemIndex = firstVisible + i;
            if (itemIndex >= menuLength) break;

            display.setCursor(10, topOffset + i * itemHeight);
            display.print(itemIndex == selected ? "> " : "  ");
            display.println(menuItems[itemIndex]);
        }

        display.display();

    });
}

// ---------------- TASK ----------------
void KitTask(void *pvParameters) {
    for (;;) {

        unsigned long now = millis();
        AppState currentState = getState();

        // sync requested state
        if (stateChangeRequested) {
            AppState newState = requestedState;

            switch (newState) {
                case APP_PPG: break;
                case LEVEL_APP: break;
                case SETTINGS: break;
                case RGB: break;
                case SNAKE: break;
                default: break;
            }

            setState(newState);
            stateChangeRequested = false;
            currentState = newState;
        }

        // INPUT (only menu)
        if (currentState == MENU) {
            if (controls.leftPressed) {
                selected = (selected - 1 + menuLength) % menuLength;

                if (selected < firstVisible) firstVisible = selected;
                if (selected == menuLength - 1) firstVisible = max(menuLength - visibleItems, 0);

                controls.leftPressed = false;
            }

            if (controls.rightPressed) {
                selected = (selected + 1) % menuLength;

                if (selected >= firstVisible + visibleItems)
                    firstVisible = selected - visibleItems + 1;

                if (selected == 0) firstVisible = 0;

                controls.rightPressed = false;
            }

            if (controls.FingerTouchedFlag() && now - lastStateChange > 300) {

                switch (selected) {
                    case 0: requestedState = APP_PPG; break;
                    case 1: requestedState = LEVEL_APP; break;
                    case 2: requestedState = SNAKE; break;
                    case 3: requestedState = RGB; break;
                    case 4: requestedState = SETTINGS; break;
                }

                stateChangeRequested = true;
                lastStateChange = now;
            }

            settings.Rainbow();

            drawMenu();
        }

        // RUN MODES
        switch (currentState) {

            case APP_PPG:
                if (!ppg.isRunning()) ppg.begin();
                if (ppg.shouldExit()) setState(MENU);
                break;

            case LEVEL_APP:
                if (!level.isRunning()) level.start();
                if (level.shouldExit()) setState(MENU);
                break;

            case SNAKE:
                if (!snake.isRunning()) {
                    snake.begin();
                }

                if (snake.shouldExit() && !snake.isRunning()) {
                    setState(MENU);
                }
                break;

            case RGB:
                if (!rgbMenu.isRunning()) rgbMenu.start();
                if (rgbMenu.shouldExit()) setState(MENU);
                break;

            case SETTINGS:
                if (!settings.isRunning()) settings.begin();
                if (settings.shouldExit()) setState(MENU);
                break;

            default:
                break;
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);

        // Debug layer
        static unsigned long lastChristmas = 0;
        if (millis() - lastChristmas >= (1*1000)) {
            lastChristmas = millis();
            Serial.printf("Heap: %u / %u bytes (%.1f%% free)\n",
              freeHeap,
              totalHeap,
              heapPercent);
        }
    }
}

// ---------------- WEB ----------------
void Web_menu() {

    web.server().on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(204);
    });

    web.server().on("/rgb", HTTP_GET, [](AsyncWebServerRequest *request) {
        requestedState = RGB;
        stateChangeRequested = true;
        request->redirect("/rgb.html");
    });

    web.server().on("/snake", HTTP_GET, [](AsyncWebServerRequest *request) {
        requestedState = SNAKE;
        stateChangeRequested = true;
        request->redirect("/snake.html");
    });

    web.server().on("/level", HTTP_GET, [](AsyncWebServerRequest *request) {
        requestedState = LEVEL_APP;
        stateChangeRequested = true;
        request->redirect("/level.html");
    });

    web.server().on("/ppg", HTTP_GET, [](AsyncWebServerRequest *request) {
        requestedState = APP_PPG;
        stateChangeRequested = true;
        request->redirect("/ppg.html");
    });

    web.server().serveStatic("/", LittleFS, "/")
        .setDefaultFile("main.html")
        .setCacheControl("max-age=600");
}

// ---------------- SETUP ----------------
void setup() {
    Serial.begin(115200);

    if(!LittleFS.begin()){
        Serial.println("LittleFS mount failed");
        return;
    }

    stateMutex = xSemaphoreCreateMutex();
    if (!stateMutex) {
        Serial.println("State mutex failed");
    }

    WiFi.onEvent(onWiFiEvent);

    web.Wifi_startup();

    while (!wifiReady) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    Serial.println("WiFi ready");

    rgbMenu.web_RGBMenu();
    snake.web_SnakeLib();
    level.web_Level();
    ppg.web_ppg();
    Web_menu();

    controls.begin();

    display.setRotation(0);
    display.clearDisplay();
    display.display();

    ppg.calibrateSensor();
    level.gyro_calib();
    settings.init();

    web.start();
    Serial.println("Web server started");

    xTaskCreatePinnedToCore(
        KitTask,
        "KitTask",
        4096,
        NULL,
        2,
        NULL,
        1
    );
}

// ---------------- LOOP ----------------
void loop() {
}