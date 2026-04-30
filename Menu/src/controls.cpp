#include "controls.h"

Controls* Controls::instance = nullptr;

Controls::Controls() : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1),
                       lastLeftTime(0), 
                       lastRightTime(0),
                       touch_baseline(0),
                       strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800),
                       rightPressed(false),
                       leftPressed(false) {
    instance = this;

    // Vytvoření mutexů pro displej a LED diodu
    displayMutex = xSemaphoreCreateMutex();
    ledMutex = xSemaphoreCreateMutex();
}

void Controls::begin() {

    // Nastavení režimu pinů
    pinMode(LEFT_PIN, INPUT);
    pinMode(RIGHT_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(IR_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(PPG_PIN, INPUT); 
    pinMode(BUZZER_PIN, OUTPUT);

    // Přiřazení přerušení k tlačítkům
    attachInterrupt(digitalPinToInterrupt(LEFT_PIN), [](){ instance->handleLeft(); }, FALLING);
    attachInterrupt(digitalPinToInterrupt(RIGHT_PIN), [](){ instance->handleRight(); }, FALLING);

    // Inicializace dotykového senzoru, displeje a adresovatelné LED
    calibrateTouch();
    initDisplay();
    strip.begin();
    strip.show();
}

void Controls::calibrateTouch() {
    long sum = 0;
    const int samples = 20;

    // Sběr vzorků pro určení výchozí hodnoty dotykového senzoru
    for (int i = 0; i < samples; i++) {
        sum += touchRead(TOUCH_PIN);
        delay(10);
    }

    // Výpočet výchozí hodnoty jako průměru naměřených vzorků
    touch_baseline = sum / samples;
}

bool Controls::fingerTouched() {

    static unsigned long fingerHoldTime = 250;

    // Načtení hodnoty senzoru a výpočet detekčního prahu
    int t = touchRead(TOUCH_PIN);
    int threshold = max(5, touch_baseline - 10);
    bool currentlyTouched = t < threshold;

    // Kontrola, jestli dotyk trvá dostatečně dlouho
    if (currentlyTouched != touchState) {
        if (lastTouchChangeTime == 0) {
            lastTouchChangeTime = millis();
        }

        if (millis() - lastTouchChangeTime >= fingerHoldTime) {
            touchState = currentlyTouched;
            lastTouchChangeTime = 0;
        }
    } else {
        lastTouchChangeTime = 0;
    }

    return touchState;
}

bool Controls::FingerTouchedFlag() {

    // Detekce pouze prvního okamžiku dotyku
    bool currentState = fingerTouched();
    bool touchedFlag = (!prevTouchFlagState && currentState);

    prevTouchFlagState = currentState;
    return touchedFlag;
}

void Controls::resetTouchState() {
    touchState = false; 
    prevTouchFlagState = false;
    lastTouchChangeTime = 0;
}

void Controls::initDisplay() { 
    
    // Inicializace OLED displeje na adrese 0x3C
    display.begin(0x3C, true);
    display.clearDisplay();
}

// Přerušení levého tlačítka s debounce
void IRAM_ATTR Controls::handleLeft() {
    unsigned long now = millis();

    if (now - lastLeftTime > debounceTime) {
        leftPressed = true;
        lastLeftTime = now;
    }
}

// Přerušení pravého tlačítka s debounce
void IRAM_ATTR Controls::handleRight() {
    unsigned long now = millis();

    if (now - lastRightTime > debounceTime) {
        rightPressed = true;
        lastRightTime = now;
    }
}

void Controls::mutexDisplay(std::function<void()> fn) {

    // Uzamčení displeje po dobu vykreslování
    if (xSemaphoreTake(displayMutex, 10 / portTICK_PERIOD_MS)) {
        fn();
        xSemaphoreGive(displayMutex);
    }
}

void Controls::mutexLed(std::function<void()> fn) {

    // Uzamčení LED diody po dobu změny barvy
    if (xSemaphoreTake(ledMutex, 10 / portTICK_PERIOD_MS)) {
        fn();
        xSemaphoreGive(ledMutex);
    }
}
