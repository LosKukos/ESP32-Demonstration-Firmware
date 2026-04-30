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

// Objekty modulů aplikace
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

// Položky hlavního menu
constexpr int MENU_LENGTH = 5;
const char* MENU_ITEMS[MENU_LENGTH] = {
    "PPG",
    "MPU",
    "Had",
    "LED",
    "Nastaveni"
};

// Nastavení zobrazení hlavního menu
constexpr int VISIBLE_ITEMS = 4;
constexpr int ITEM_HEIGHT = 12;
constexpr int TOP_OFFSET = 20;
constexpr unsigned long INPUT_COOLDOWN_MS = 300;

// Požadavek na změnu stavu aplikace
volatile AppState pendingState = MENU;
volatile bool stateChangeRequested = false;
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

// Statická paměť pro hlavní FreeRTOS task
StaticTask_t mainTaskBuffer;
StackType_t mainTaskStack[4096];

// Stav aplikace
AppState currentState = CALIBRATION;
AppState transitionTarget = MENU;
bool transitionInProgress = false;
bool calibrationDone = false;

// Stav hlavního menu
int selectedItem = 0;
int firstVisibleItem = 0;
unsigned long lastSelectionTime = 0;

void requestStateChange(AppState newState) {
    // Uložení požadavku na změnu stavu
    portENTER_CRITICAL(&stateMux);
    pendingState = newState;
    stateChangeRequested = true;
    portEXIT_CRITICAL(&stateMux);
}

bool consumeStateRequest(AppState& outState) {
    bool hasRequest = false;

    // Zpracování požadavku na změnu stavu
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
    // Požadavek na ukončení všech modulů
    ppg.requestStop();
    level.requestStop();
    snake.requestStop();
    rgbMenu.requestStop();
    settings.requestStop();
}

bool anyModuleRunning() {
    // Kontrola běhu modulů
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
    // Posun výběru v hlavním menu
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
    // Převod položky menu na stav aplikace
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

    // Zpracování tlačítek v hlavním menu
    if (controls.leftPressed) {
        updateMenuSelection(-1);
        controls.leftPressed = false;
    }

    if (controls.rightPressed) {
        updateMenuSelection(1);
        controls.rightPressed = false;
    }

    // Potvrzení výběru dotykovým senzorem
    if (controls.FingerTouchedFlag() &&
        (now - lastSelectionTime >= INPUT_COOLDOWN_MS)) {
        requestStateChange(menuSelectionToState());
        lastSelectionTime = now;
    }
}

void startModuleForState(AppState state) {
    // Spuštění modulu podle aktuálního stavu aplikace
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
                level.begin();
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
                rgbMenu.begin();
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
    // Kalibrace měřicích modulů po startu zařízení
    ppg.calibrateSensor();
    level.calibrate();
}

void updateModuleLifecycle() {
    // Provedení úvodní kalibrace
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

    // Kontrola běhu a ukončení aktivního modulu
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

    // Zpracování požadavku na změnu stavu
    if (!consumeStateRequest(requested)) {
        return;
    }

    if (requested == currentState && !transitionInProgress) {
        return;
    }

    // Přímé spuštění modulu z hlavního menu
    if (currentState == MENU && !anyModuleRunning()) {
        currentState = requested;
        startModuleForState(currentState);
        return;
    }

    // Při přechodu mezi moduly se nejprve ukončí běžící modul
    requestStopAllModules();
    transitionTarget = requested;
    transitionInProgress = true;
}

void processTransition() {
    if (!transitionInProgress) {
        return;
    }

    // Čekání na ukončení aktivních modulů
    if (anyModuleRunning()) {
        return;
    }

    controls.resetTouchState();
    currentState = transitionTarget;
    transitionInProgress = false;
    startModuleForState(currentState);
}

void registerWebRoutes() {
    // Registrace webových endpointů jednotlivých modulů
    rgbMenu.registerWebRoutes();
    snake.registerWebRoutes();
    level.registerWebRoutes();
    ppg.registerWebRoutes();

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

    // Poskytování statických souborů z LittleFS
    settings.server().serveStatic("/", LittleFS, "/")
        .setDefaultFile("main.html")
        .setCacheControl("no-store, no-cache, must-revalidate, max-age=0");
}

void initStorage() {
    // Inicializace souborového systému
    LittleFS.begin();
}

void initHardware() {
    // Inicializace hardwarové vrstvy
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

        // Obsluha hlavního menu
        if (currentState == MENU && !transitionInProgress) {
            handleMenuInput();
            settings.updateRainbow();
            drawMenu();
        }

        updateModuleLifecycle();
        vTaskDelay(pdMS_TO_TICKS(50));
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

    // Spuštění WiFi přístupového bodu a webového rozhraní
    settings.switchConnectivity(Settings::ConnectivityMode::WIFI);

    // Vytvoření hlavního tasku aplikace
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
