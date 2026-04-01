#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <Wire.h>
#include <WiFi.h>

#include <SnakeLib.h>
#include <RGBMENU.h>
#include <controls.h>
#include <settings.h>
#include <PPG.h>
#include <Level.h>

namespace {

Controls controls;
Settings settings(controls);
SnakeLib snake(controls, settings);
RGBMenu rgbMenu(controls, settings);
PPG ppg(controls, settings);
Level level(controls, settings);

enum AppState {
    CALIBRATION,
    MENU,
    APP_PPG,
    LEVEL_APP,
    SNAKE,
    RGB,
    SETTINGS_APP
};

constexpr int MENU_LENGTH = 5;
const char* MENU_ITEMS[MENU_LENGTH] = {
    "PPG",
    "Gyroskop",
    "Had",
    "Ovladani LED",
    "Nastaveni"
};

constexpr int VISIBLE_ITEMS = 4;
constexpr int ITEM_HEIGHT = 12;
constexpr int TOP_OFFSET = 20;
constexpr unsigned long INPUT_COOLDOWN_MS = 300;

volatile AppState pendingState = MENU;
volatile bool stateChangeRequested = false;
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

StaticTask_t mainTaskBuffer;
StackType_t mainTaskStack[4096];

AppState currentState = CALIBRATION;
AppState transitionTarget = MENU;
bool transitionInProgress = false;
bool calibrationDone = false;

int selectedItem = 0;
int firstVisibleItem = 0;
unsigned long lastSelectionTime = 0;

void requestStateChange(AppState newState) {
    portENTER_CRITICAL(&stateMux);
    pendingState = newState;
    stateChangeRequested = true;
    portEXIT_CRITICAL(&stateMux);
}

bool consumeStateRequest(AppState& outState) {
    bool hasRequest = false;

    portENTER_CRITICAL(&stateMux);
    if (stateChangeRequested) {
        outState = pendingState;
        stateChangeRequested = false;
        hasRequest = true;
    }
    portEXIT_CRITICAL(&stateMux);

    return hasRequest;
}

void requestStopAllModules() {
    ppg.requestStop();
    level.requestStop();
    snake.requestStop();
    rgbMenu.requestStop();
    settings.requestStop();
}

bool anyModuleRunning() {
    return ppg.isRunning() ||
           level.isRunning() ||
           snake.isRunning() ||
           rgbMenu.isRunning() ||
           settings.isRunning();
}

void drawMenu() {
    controls.mutexDisplay([&]() {
        auto& display = controls.display;

        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);
        display.setCursor((SCREEN_WIDTH - 8 * 11) / 2, 0);
        display.println("HLAVNI MENU");

        for (int i = 0; i < VISIBLE_ITEMS; ++i) {
            const int itemIndex = firstVisibleItem + i;
            if (itemIndex >= MENU_LENGTH) {
                break;
            }

            display.setCursor(10, TOP_OFFSET + i * ITEM_HEIGHT);
            display.print(itemIndex == selectedItem ? "> " : "  ");
            display.println(MENU_ITEMS[itemIndex]);
        }

        display.display();
    });
}

void updateMenuSelection(int direction) {
    selectedItem = (selectedItem + direction + MENU_LENGTH) % MENU_LENGTH;

    if (selectedItem < firstVisibleItem) {
        firstVisibleItem = selectedItem;
    }

    if (selectedItem >= firstVisibleItem + VISIBLE_ITEMS) {
        firstVisibleItem = selectedItem - VISIBLE_ITEMS + 1;
    }

    if (selectedItem == 0) {
        firstVisibleItem = 0;
    }

    if (selectedItem == MENU_LENGTH - 1) {
        firstVisibleItem = max(MENU_LENGTH - VISIBLE_ITEMS, 0);
    }
}

AppState menuSelectionToState() {
    switch (selectedItem) {
        case 0: return APP_PPG;
        case 1: return LEVEL_APP;
        case 2: return SNAKE;
        case 3: return RGB;
        case 4: return SETTINGS_APP;
        default: return MENU;
    }
}

void handleMenuInput() {
    const unsigned long now = millis();

    if (controls.leftPressed) {
        updateMenuSelection(-1);
        controls.leftPressed = false;
    }

    if (controls.rightPressed) {
        updateMenuSelection(1);
        controls.rightPressed = false;
    }

    if (controls.FingerTouchedFlag() &&
        (now - lastSelectionTime >= INPUT_COOLDOWN_MS)) {
        requestStateChange(menuSelectionToState());
        lastSelectionTime = now;
    }
}

void startModuleForState(AppState state) {
    switch (state) {
        case APP_PPG:
            if (!ppg.isRunning()) {
                controls.resetTouchState();
                ppg.begin();
            }
            break;

        case LEVEL_APP:
            if (!level.isRunning()) {
                controls.resetTouchState();
                level.start();
            }
            break;

        case SNAKE:
            if (!snake.isRunning()) {
                controls.resetTouchState();
                snake.begin();
            }
            break;

        case RGB:
            if (!rgbMenu.isRunning()) {
                controls.resetTouchState();
                rgbMenu.start();
            }
            break;

        case SETTINGS_APP:
            if (!settings.isRunning()) {
                controls.resetTouchState();
                settings.begin();
            }
            break;

        case MENU:
        case CALIBRATION:
        default:
            break;
    }
}

void runStartupCalibration() {
    ppg.calibrateSensor();
    level.gyro_calib();
}

void updateModuleLifecycle() {
    if (currentState == CALIBRATION) {
        if (!calibrationDone) {
            runStartupCalibration();
            calibrationDone = true;
        } else {
            controls.resetTouchState();
            currentState = MENU;
        }
        return;
    }

    switch (currentState) {
        case APP_PPG:
            if (ppg.shouldExit()) {
                if (!ppg.isRunning()) {
                    controls.resetTouchState();
                    currentState = MENU;
                }
            } else if (!ppg.isRunning()) {
                startModuleForState(currentState);
            }
            break;

        case LEVEL_APP:
            if (level.shouldExit()) {
                if (!level.isRunning()) {
                    controls.resetTouchState();
                    currentState = MENU;
                }
            } else if (!level.isRunning()) {
                startModuleForState(currentState);
            }
            break;

        case SNAKE:
            if (snake.shouldExit()) {
                if (!snake.isRunning()) {
                    controls.resetTouchState();
                    currentState = MENU;
                }
            } else if (!snake.isRunning()) {
                startModuleForState(currentState);
            }
            break;

        case RGB:
            if (rgbMenu.shouldExit()) {
                if (!rgbMenu.isRunning()) {
                    controls.resetTouchState();
                    currentState = MENU;
                }
            } else if (!rgbMenu.isRunning()) {
                startModuleForState(currentState);
            }
            break;

        case SETTINGS_APP:
            if (settings.shouldExit()) {
                if (!settings.isRunning()) {
                    controls.resetTouchState();
                    currentState = MENU;
                }
            } else if (!settings.isRunning()) {
                startModuleForState(currentState);
            }
            break;

        case MENU:
        case CALIBRATION:
        default:
            break;
    }
}

void processStateRequests() {
    AppState requested;
    if (!consumeStateRequest(requested)) {
        return;
    }

    if (requested == currentState && !transitionInProgress) {
        return;
    }

    if (currentState == MENU && !anyModuleRunning()) {
        currentState = requested;
        startModuleForState(currentState);
        return;
    }

    requestStopAllModules();
    transitionTarget = requested;
    transitionInProgress = true;
}

void processTransition() {
    if (!transitionInProgress) {
        return;
    }

    if (anyModuleRunning()) {
        return;
    }

    controls.resetTouchState();
    currentState = transitionTarget;
    transitionInProgress = false;
    startModuleForState(currentState);
}

void registerWebRoutes() {
    rgbMenu.web_RGBMenu();
    snake.web_SnakeLib();
    level.web_Level();
    ppg.web_ppg();

    settings.server().on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(204);
    });

    settings.server().on("/rgb", HTTP_GET, [](AsyncWebServerRequest* request) {
        requestStateChange(RGB);
        request->redirect("/rgb.html");
    });

    settings.server().on("/snake", HTTP_GET, [](AsyncWebServerRequest* request) {
        requestStateChange(SNAKE);
        request->redirect("/snake.html");
    });

    settings.server().on("/level", HTTP_GET, [](AsyncWebServerRequest* request) {
        requestStateChange(LEVEL_APP);
        request->redirect("/level.html");
    });

    settings.server().on("/ppg", HTTP_GET, [](AsyncWebServerRequest* request) {
        requestStateChange(APP_PPG);
        request->redirect("/ppg.html");
    });

    settings.server().serveStatic("/", LittleFS, "/")
        .setDefaultFile("main.html")
        .setCacheControl("no-store, no-cache, must-revalidate, max-age=0");
}

void initStorage() {
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
        while (true) {
            delay(1000);
        }
    }
}

void initHardware() {
    controls.begin();
    controls.display.setRotation(0);
    controls.display.clearDisplay();
    controls.display.display();
}

void mainTask(void* pvParameters) {
    (void)pvParameters;

    for (;;) {
        processStateRequests();
        processTransition();

        if (currentState == MENU && !transitionInProgress) {
            handleMenuInput();
            settings.Rainbow();
            drawMenu();
        }

        updateModuleLifecycle();
        vTaskDelay(pdMS_TO_TICKS(50));

        static unsigned long lastDbg = 0;
        if (millis() - lastDbg >= 2000) {
            lastDbg = millis();
            Serial.printf("[MAIN] heap=%u minHeap=%u stackFree=%u\n",
                ESP.getFreeHeap(),
                ESP.getMinFreeHeap(),
                uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t));
        }
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(300);

    initStorage();
    initHardware();

    settings.init();
    registerWebRoutes();

    // Default po startu: WiFi AP + async web
    settings.switchConnectivity(Settings::ConnectivityMode::WIFI_WEB);

    xTaskCreateStaticPinnedToCore(
        mainTask,
        "MainTask",
        4096,
        nullptr,
        2,
        mainTaskStack,
        &mainTaskBuffer,
        1
    );
}

void loop() {
}