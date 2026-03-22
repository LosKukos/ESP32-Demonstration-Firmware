#include "controls.h"

// singleton instance
Controls* Controls::instance = nullptr;

// -----------------------------
// --- Konstruktor ---
// -----------------------------
Controls::Controls() : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1),
                       lastLeftTime(0),
                       lastRightTime(0),
                       touch_baseline(0),
                       strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800),
                       EncoderButton(false),
                       EncoderRight(false),
                       EncoderLeft(false),
                       rightPressed(false),
                       leftPressed(false) {
    instance = this;
}

// -----------------------------
// --- Inicializace ---
// -----------------------------
void Controls::begin() {

    // --- Nastavení režimu pinů ---
    pinMode(LEFT_PIN, INPUT);
    pinMode(RIGHT_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(ENC_SW_PIN, INPUT_PULLUP);
    pinMode(ENC_CLK_PIN, INPUT);
    pinMode(ENC_DT_PIN, INPUT);


    // --- Přiřazení interruptů k jednotlivým pinům ---
    attachInterrupt(digitalPinToInterrupt(LEFT_PIN), [](){ instance->handleLeft(); }, FALLING); // Left pin - D16
    attachInterrupt(digitalPinToInterrupt(RIGHT_PIN), [](){ instance->handleRight(); }, FALLING); // Right pin - D17
    attachInterrupt(digitalPinToInterrupt(ENC_SW_PIN), [](){ instance->handleEncoderButton(); }, FALLING); // Tlačítko enkódéru - D12
    attachInterrupt(digitalPinToInterrupt(ENC_CLK_PIN), [](){ instance->handleEncoder(); }, CHANGE); // Enkódér - D32
    attachInterrupt(digitalPinToInterrupt(ENC_DT_PIN), [](){ instance->handleEncoder(); }, CHANGE); // Enkódér - D33

    // --- Inicializace periferií ---
    calibrateTouch();
    initDisplay();
    strip.begin();
    strip.show();
}

// -----------------------------
// --- Funkce ---
// -----------------------------

void Controls::calibrateTouch() {
    long sum = 0;
    const int samples = 20;
    for (int i = 0; i < samples; i++) {
        sum += touchRead(TOUCH_PIN);
        delay(10);
    }
    touch_baseline = sum / samples;
    Serial.print("Touch baseline calibrated: ");
    Serial.println(touch_baseline);
}

bool Controls::fingerTouched() {
    static bool state = false;
    static unsigned long lastChangeTime = 0;
    static unsigned long fingerHoldTime = 250;

    int t = touchRead(TOUCH_PIN);
    int threshold = max(5, touch_baseline - 10);

    bool currentlyTouched = t < threshold;

    if (currentlyTouched != state) {
        if (lastChangeTime == 0) lastChangeTime = millis();
        if (millis() - lastChangeTime >= fingerHoldTime) {
            state = currentlyTouched;
            lastChangeTime = 0;
        }
    } else {
        lastChangeTime = 0;
    }

    return state;
}

bool Controls::FingerTouchedFlag() {
    static bool prevState = false;
    bool currentState = fingerTouched(); // aktuální stav prstu po hysterzi
    bool FingerTouchedFlag = (!prevState && currentState); // pouze první přiložení
    prevState = currentState;
    return FingerTouchedFlag;
}

void Controls::initDisplay() {
    if (!display.begin()) {
        Serial.println("OLED init failed!");
        while(1);
    }
    display.clearDisplay();
}



// -----------------------------
// --- ISR ---
// -----------------------------
void Controls::handleLeft() {
    unsigned long now = millis();
    if (now - lastLeftTime > debounceTime) {
        leftPressed = true;
        lastLeftTime = now;
    }
}

void Controls::handleRight() {
    unsigned long now = millis();
    if (now - lastRightTime > debounceTime) {
        rightPressed = true;
        lastRightTime = now;
    }
}

void Controls::handleEncoderButton() {
    unsigned long now = millis();
    if (now - lastEncButtonTime > debounceTime) {
        EncoderButton = true;
        lastEncButtonTime = now;
    }
}

void IRAM_ATTR Controls::handleEncoder() {
    static uint8_t lastState = 0;          // předchozí stav CLK/DT
    static unsigned long lastEncTime = 0;  // pro debounce

    unsigned long now = millis();
    if (now - lastEncTime < debounceTimeEnc) return;     // jemný debounce 2 ms
    lastEncTime = now;

    // aktuální stav: bit1 = CLK, bit0 = DT
    uint8_t currState = (digitalRead(ENC_CLK_PIN) << 1) | digitalRead(ENC_DT_PIN);

    // stavový automat
    switch ((lastState << 2) | currState) {

        // RIGHT (Clockwise)
        case 0b0001: case 0b0111: case 0b1110: case 0b1000:
            EncoderRight = true;
            break;

        // LEFT (Counter Clockwise)
        case 0b0010: case 0b1011: case 0b1101: case 0b0100:
            EncoderLeft = true;
            break;

        // všechno ostatní = bounce / glitch
        default:
            break;
    }

    lastState = currState;
}
