#include "controls.h"

// Singleton instance, zajiťuje pouze jednu instanci třídy Controls v programu
Controls* Controls::instance = nullptr;

// Konstruktor: vytváří objekt Controls a nastavuje všechno potřebné (displej, LED, tlačítka) a zároveň dává vědět, že tahle instance je ta jediná.
Controls::Controls() : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1),
                       lastLeftTime(0), 
                       lastRightTime(0),
                       touch_baseline(0),
                       strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800),
                       rightPressed(false),
                       leftPressed(false) {
    instance = this;

    displayMutex = xSemaphoreCreateMutex();
    ledMutex = xSemaphoreCreateMutex();
}

// --- Funkce pro inicializaci ESP32 ---

void Controls::begin() {

    // --- Nastavení režimu pinů na vstup/výstup ---
    pinMode(LEFT_PIN, INPUT);
    pinMode(RIGHT_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(IR_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(PPG_PIN, INPUT); 
    pinMode(BUZZER_PIN, OUTPUT);


    // --- Přiřazení přerušení k jednotlivým pinům ---
    attachInterrupt(digitalPinToInterrupt(LEFT_PIN), [](){ instance->handleLeft(); }, FALLING); // Left pin - D16
    attachInterrupt(digitalPinToInterrupt(RIGHT_PIN), [](){ instance->handleRight(); }, FALLING); // Right pin - D17

    // --- Inicializace periferií a kalibrace ---
    calibrateTouch();
    initDisplay();
    strip.begin();
    strip.show();
}

// --- Pomocné funkce pro celý program ---

// Ve funkci calibrateTouch dochází ke kalibraci kapacitního dotykového snímače
// umístěného na Touch Pin D04. Dochází zde k měření 20 vzorků, které jsou následovně zprůměrovány
// a nastaveny jako výchozí hodnota
void Controls::calibrateTouch() {
    long sum = 0;
    const int samples = 20; // Počet vzorků

    for (int i = 0; i < samples; i++) { // Smyčka pro čtení snímače a ščítání sumy
        sum += touchRead(TOUCH_PIN);
        delay(10);
    }

    touch_baseline = sum / samples;
    // Suma hodnot z snímače je vydělena počtem vzorků, vytvořený průměr a ten nastavený jako
    // nová výchozí hodnota
}

// V této funkci zjišťujeme přiložení prstu na kapacitní snímač. Sledujeme délku přiložení
// a překročení prahu pro zaznamenání stisku
bool Controls::fingerTouched() {

    static bool state = false;
    static unsigned long lastChangeTime = 0; // Poslední čas změny
    static unsigned long fingerHoldTime = 250; // Doba po kterou musí prst přiložen, aby jsme mohli uvažovat
    // o stisku a ne náhodném dotyku

    int t = touchRead(TOUCH_PIN); // Čtení pinu pro zjištěná hodnoty snímače
    int threshold = max(5, touch_baseline - 10); // Nastavení prahu na minimální hodnotu 5 a nebo dopočtenou hodnotu
    // z touch_baseline - 10

    bool currentlyTouched = t < threshold; // Detekce, jestli hodnota snímače překročila práh

    // Smyčka, která detekuje délku doteku. Slouží k zajištění, že se doopravdy jedná o stisk a ne náhodný dotek
    if (currentlyTouched != state) { // Detekce, jestli se aktuální stav liší od posledního uloženého stavu
        if (lastChangeTime == 0) lastChangeTime = millis(); // Pokud nebyl nastavený poslední čas změny,
        // tak ho nastavíme na aktuální čas prohramu od startu
        if (millis() - lastChangeTime >= fingerHoldTime) { // Pokud uplynulo více času než je doba pro přiložení prstu,
            // dojde k nastavení stavu na dotyk (currentlyTouched) a zároveň dojde k restarování času
            state = currentlyTouched;
            lastChangeTime = 0;
        }
    } else {
        lastChangeTime = 0; // Pokud odpovídá poslední uložený čas aktuálnímu času, dojde k restartování
    }

    return state; // Vrácení hodnoty stavu, jestli došlo k splnění všech podmínek a dotyku
}

// Tato funkce využívá předchozí funkce, která detekuje přiložení prstu a stará se o to, aby jsme registrovali
// pouze první dotek a nedocházelo k opakování při delším doteku
bool Controls::FingerTouchedFlag() { // Nastavení značky, pro dotyk pro použití dále v programu

    static bool prevState = false; // Předchozí uložený stav 
    bool currentState = fingerTouched(); // Aktuální stav přiložení z funkce fingerTouched()
    bool FingerTouchedFlag = (!prevState && currentState); // Detekce pouze prvního přiložení,
    // aby nedošlo k opakovanému spínání jediným dotykem
    prevState = currentState; // Nastavení aktuálního stavu na předchozí
    return FingerTouchedFlag; // Vrácení hodnoty 
}

// Inicializace OLED displaye 
void Controls::initDisplay() { 
    
    bool displayReady = display.begin(0x3C, true); // Uložení výsledku inicializace displaye

    if (displayReady == false) { // Pokud dojde k selhání inicializace OLED displaye, 
        // kód zde zůstane a bude vypisovat chybu do sériové linky
        while(true) { 
            Serial.println("Chyba inicializace OLED");
        }
    }
    display.clearDisplay(); // Vyčistění displaye po inicializaci
}

// --- Interrupt Service Routine ---
// Tyto funkce slouží k okamžitému přerušení programu, jsou spouštěné hardwarovými prvky, 
// v našem případě tlačítka a enkódér. Při přerušení dojde k okamžitému provedení kódu
// a následnému vrácení do původního kódu

// Funkce přerušení pro levé tlačítko, odpovídající D16. Dojde ke kontrole debounce,
// aby nedocházelo k opakovanému spínání tlačítka
void IRAM_ATTR Controls::handleLeft() {
    unsigned long now = millis();
    if (now - lastLeftTime > debounceTime) {
        leftPressed = true;
        lastLeftTime = now;
    }
}

// Funkce přerušení pro pravé tlačítko, odpovídající D17. Dojde ke kontrole debounce,
// aby nedocházelo k opakovanému spínání tlačítka
void IRAM_ATTR Controls::handleRight() {
    unsigned long now = millis();
    if (now - lastRightTime > debounceTime) {
        rightPressed = true;
        lastRightTime = now;
    }
}

void Controls::mutexDisplay(std::function<void()> fn) {
    if (xSemaphoreTake(displayMutex, 10/portTICK_PERIOD_MS)) {
        fn();
        xSemaphoreGive(displayMutex);
    }
}

void Controls::mutexLed(std::function<void()> fn) {
    if (xSemaphoreTake(ledMutex, 10/portTICK_PERIOD_MS)) {
        fn();
        xSemaphoreGive(ledMutex);
    }
}