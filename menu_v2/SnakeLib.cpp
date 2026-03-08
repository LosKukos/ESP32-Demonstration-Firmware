#include "SnakeLib.h"

// --- Statická instance pro interrupty ---
SnakeLib* SnakeLib::_instance = nullptr;

// --- Konstruktor knihovny ---
    SnakeLib::SnakeLib(Controls& ctrl) : controls(ctrl) {
        // Nastavení polohy v menu, počtu položek a názvů v menu
        _gameState = MENU;
        _menuIndex = 0;
        _menuItemCount = 3;
        _menuItems[0] = "Start";
        _menuItems[1] = "Nastaveni";
        _menuItems[2] = "Konec";

        // Nastavení obtížností - jejich názvů a rychlosti
        _difficultyIndex = 1;
        _difficultyLevels[0] = "Lehka";
        _difficultyLevels[1] = "Stredni";
        _difficultyLevels[2] = "Tezka";
        _difficultySpeeds[0] = 200;
        _difficultySpeeds[1] = 150;
        _difficultySpeeds[2] = 100;
        _gameSpeed = _difficultySpeeds[_difficultyIndex];

        _exit = false;
    }


// --- Inicializace knihovny hada ---
    void SnakeLib::begin() {
        // --- Inicializace bzučáku ---
        pinMode(controls.BUZZER_PIN, OUTPUT);

        // --- Inicializace pozice hada ---
        _snake[0] = {128/8, 64/8};  // Střed displeje
        _snakeLength = 2;   // Počáteční délka hada
        _dirX = 1; // Směr v ose X
        _dirY = 0; // Směr v ose Y

        _lastMoveTime = millis(); // Proměnná pro uložení času posledního pohybu

        _exit = false; // Proměnná pro odchod ze hry

        _menuIndex = 0; // Reset indexu menu
}

// --- Vlastní smyčka hry ---
    void SnakeLib::loop() {
    // Ukončení hry
        if (_exit) {
            controls.strip.setPixelColor(0, 0, 0, 0); // zhasnutí LED při odchodu
            controls.strip.show();
            return;
        }

    // --- Zpracování dotyku ---
        if (controls.FingerTouchedFlag()) {
            handleTouchConfirm();
        }

    // --- Zpracování tlačítek ---
        if (controls.leftPressed) {
            handleButtonPress();
        }
        if (controls.rightPressed) {
            handleButtonPress();
        }

    // --- Hlavní stavový automat ---
    switch (_gameState) {
        // Hlavní menu - vykreslení jednotlivých položek menu
            case MENU:
            controls.strip.setPixelColor(0, 0, 255, 255);
            controls.strip.show();
            controls.display.clearDisplay();
            controls.display.setTextSize(1);
            controls.display.setTextColor(SH110X_WHITE);
            controls.display.setCursor(30, 10);
            controls.display.println("== MENU ==");
            for (int i = 0; i < _menuItemCount; i++) {
                controls.display.setCursor(40, 25 + i * 12);
                if (i == _menuIndex) controls.display.print("> ");
                controls.display.println(_menuItems[i]);
            }
            controls.display.display();
            break;

        // Nastavení hry - vykreslení menu nastavení a možnost výběru obtížnosti
            case SETTINGS:
            controls.display.clearDisplay();
            controls.display.setTextSize(1);
            controls.display.setTextColor(SH110X_WHITE);
            controls.display.setCursor(25, 10);
            controls.display.println("Obtiznost:");
            controls.display.setCursor(40, 25);
            controls.display.print("> ");
            controls.display.println(_difficultyLevels[_difficultyIndex]);
            controls.display.display();
            updateDifficultyLED();
            break;

        // Samotná hra
            case GAME:
                if (millis() - _lastMoveTime >= _gameSpeed) {
                _lastMoveTime = millis();

                for (int i = _snakeLength - 1; i > 0; i--)
                    _snake[i] = _snake[i - 1];

                _snake[0].x += _dirX;
                _snake[0].y += _dirY;

                // Kolize s hranami - pokud by had naboural do okraje mapy, dojde ke konci hry
                if (_snake[0].x < 1 || _snake[0].x >= 128/4 - 1 || // Kontrola v ose X
                    _snake[0].y < 1 || _snake[0].y >= 64/4 - 1) { // Kontrola v ose Y
                        gameOver();
                    }

                // Kolize s tělem - pokud by had měl nabourat sám do sebe, dojde ke konci hry
                for (int i = 1; i < _snakeLength; i++) {
                    if (_snake[0].x == _snake[i].x && _snake[0].y == _snake[i].y){
                        gameOver();
                    }
                }

                // Sběr ovoce - pokud hlava hada najede na pole obsahující ovoce, dojde k zvýšení délky hada a haptické odezvě pomocí bzučáku
                if (_snake[0].x == _fruit.x && _snake[0].y == _fruit.y) {
                    _snakeLength++;
                    spawnFruit();
                    digitalWrite(controls.BUZZER_PIN, HIGH);
                    delay(50);
                    digitalWrite(controls.BUZZER_PIN, LOW);
                }
                }

                // Vykreslení hry
                controls.display.clearDisplay();
                controls.display.drawRect(0, 0, 128, 64, SH110X_WHITE); // Vykerslení okraje mapy
                for (int i = 0; i < _snakeLength; i++)
                controls.display.fillRect(_snake[i].x * 4, _snake[i].y * 4, 4, 4, SH110X_WHITE); // Vykreslení hada
                controls.display.fillRect(_fruit.x * 4, _fruit.y * 4, 4, 4, SH110X_WHITE); // Vykreslení ovoce
                controls.display.display();
                break;

        // Konec hry - pokud dojde ke konci hry z důvodu střetu s okrajem mapy anebo tělem hada, dojde k ukončení hry
            case GAME_OVER:
            controls.display.clearDisplay();
            controls.display.setTextSize(1);
            controls.display.setTextColor(SH110X_WHITE);
            controls.display.setCursor(35, 25);
            controls.display.println("GAME OVER");
            controls.display.setCursor(7, 40);
            controls.display.println("Dotkni se pro navrat");
            controls.display.display();
            break;
    }
}


// --- Ovládání hry ---
    void SnakeLib::handleButtonPress() {
    switch (_gameState) {
        // Pohyb v menu
        case MENU:
            if (controls.rightPressed){
                _menuIndex = (_menuIndex + 1) % _menuItemCount;
                controls.rightPressed = false;
            }

            if (controls.leftPressed){
                _menuIndex = (_menuIndex - 1 + _menuItemCount) % _menuItemCount;
                controls.leftPressed = false;
            }
            break;

        // Pohyb v nastavení
        case SETTINGS:
            if (controls.leftPressed){
                _difficultyIndex = (_difficultyIndex + 1) % 3;
                updateDifficultyLED();
                controls.leftPressed = false;
            }

            if (controls.rightPressed){
                _difficultyIndex = (_difficultyIndex - 1 + 3) % 3;
                updateDifficultyLED();
                controls.rightPressed = false;
            }
            break;
        
        // Ovládání ve hře a změna směru hada
        case GAME:
            if (controls.leftPressed) {
                int oldX = _dirX;
                _dirX = _dirY;
                _dirY = -oldX;
                controls.leftPressed = false;
            }
            if (controls.rightPressed) {
                int oldX = _dirX;
                _dirX = -_dirY;
                _dirY = oldX;
                controls.rightPressed = false;
            }
            break;
    }
    }

// --- Restart hry ---
    void SnakeLib::resetGame() {
    // WS2812 barva podle obtížnosti
    switch (_difficultyIndex) {
        case 0: controls.strip.setPixelColor(0, 0, 255, 0); break;
        case 1: controls.strip.setPixelColor(0, 255, 165, 0); break;
        case 2: controls.strip.setPixelColor(0, 255, 0, 0); break;
    }
    controls.strip.show();

    // Reset hada
    _snake[0] = {128 / 8, 64 / 8};
    _snakeLength = 2;
    _dirX = 1;
    _dirY = 0;

    spawnFruit();

    _gameState = GAME;
    _lastMoveTime = millis();
    }

void SnakeLib::gameOver() {
    for (int i = 0; i < 3; i++) {
        digitalWrite(controls.BUZZER_PIN, HIGH);
        delay(100);
        digitalWrite(controls.BUZZER_PIN, LOW);
        delay(100);
    }
    _gameState = GAME_OVER;
}

void SnakeLib::spawnFruit() {
    bool valid = false;
    while (!valid) {
        _fruit.x = random(1, 128/4 - 2);
        _fruit.y = random(1, 64/4 - 2);
        valid = true;
        for (int i = 0; i < _snakeLength; i++) {
            if (_snake[i].x == _fruit.x && _snake[i].y == _fruit.y) {
                valid = false;
                break;
            }
        }
    }
}

void SnakeLib::handleTouchConfirm() {
    switch (_gameState) {
        case MENU:
            if (_menuIndex == 0) {
                resetGame();
            } else if (_menuIndex == 1) {
                _gameState = SETTINGS;
            } else if (_menuIndex == 2) {
                _exit = true;     // Exit
            }
            break;

        case SETTINGS:
            _gameSpeed = _difficultySpeeds[_difficultyIndex];
            _gameState = MENU;
            break;

        case GAME_OVER:
            _gameState = MENU;
            _menuIndex = 0;
            break;
    }
}

bool SnakeLib::shouldExit() {
    return _exit;
}

void SnakeLib::updateDifficultyLED() {
    switch (_difficultyIndex) {
        case 0: controls.strip.setPixelColor(0, 0, 255, 0); break; // Lehká
        case 1: controls.strip.setPixelColor(0, 255, 165, 0); break; // Střední
        case 2: controls.strip.setPixelColor(0, 255, 0, 0); break; // Těžká
    }
    controls.strip.show();
}
