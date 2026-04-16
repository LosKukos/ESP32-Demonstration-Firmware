#include "SnakeLib.h"

#include <algorithm>
#include <cstring>

namespace {
constexpr uint32_t SNAKE_TASK_STACK_WORDS = 2048;
StaticTask_t snakeTaskBuffer;
StackType_t snakeTaskStack[SNAKE_TASK_STACK_WORDS];
}

const char* const SnakeLib::MENU_ITEMS[SnakeLib::MENU_ITEM_COUNT] = {
    "Start",
    "Nastaveni",
    "Konec"
};

const char* const SnakeLib::DIFFICULTY_LEVELS[SnakeLib::DIFFICULTY_COUNT] = {
    "Lehka",
    "Stredni",
    "Tezka",
    "Chaos"
};

const int SnakeLib::DIFFICULTY_SPEEDS[SnakeLib::DIFFICULTY_COUNT] = {
    200,
    150,
    100,
    200
};

SnakeLib::SnakeLib(Controls& ctrl, Settings& settingsRef)
    : controls(ctrl), settings(settingsRef) {
    dataMutex = xSemaphoreCreateMutex();
}

bool SnakeLib::lockData(TickType_t ticks) {
    return dataMutex != nullptr && xSemaphoreTake(dataMutex, ticks) == pdTRUE;
}

void SnakeLib::unlockData() {
    if (dataMutex != nullptr) {
        xSemaphoreGive(dataMutex);
    }
}

void SnakeLib::begin() {
    if (lockData()) {
        _exit = false;
        running = true;
        _menuIndex = 0;
        _gameState = MENU;
        _gameSpeed = DIFFICULTY_SPEEDS[_difficultyIndex];
        invalidatePendingResult();
        _buzzerEndTime = 0;
        _gameOverBeepUntil = 0;
        unlockData();
    } else {
        _exit = false;
        running = true;
    }

    if (taskHandle == nullptr) {
        loadLeaderboard();
        taskHandle = xTaskCreateStaticPinnedToCore(
            task,
            "SnakeTask",
            SNAKE_TASK_STACK_WORDS,
            this,
            1,
            snakeTaskStack,
            &snakeTaskBuffer,
            1
        );

        if (taskHandle == nullptr) {
            running = false;
            return;
        }
    } else {
        vTaskResume(taskHandle);
    }
}

void SnakeLib::task(void *pvParameters) {
    SnakeLib* self = static_cast<SnakeLib*>(pvParameters);

    for (;;) {
        if (!self->running) {
            vTaskSuspend(nullptr);
        }

        if (self->_exit) {
            digitalWrite(self->controls.BUZZER_PIN, LOW);

            if (self->lockData()) {
                self->_buzzerEndTime = 0;
                self->_gameOverBeepUntil = 0;
                self->unlockData();
            }

            self->running = false;
            vTaskSuspend(nullptr);
            continue;
        }

        if (self->controls.FingerTouchedFlag()) {
            self->handleTouchConfirm();
        }

        if (self->controls.leftPressed || self->controls.rightPressed) {
            self->handleButtonPress();
        }

        self->updateBuzzer();

        switch (self->_gameState) {
            case MENU:
                self->drawMenu();
                break;

            case SETTINGS:
                self->drawSettings();
                break;

            case GAME:
                self->runGameLogic();
                self->drawGame();
                break;

            case GAME_OVER:
                self->drawGameOver();
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void SnakeLib::runGameLogic() {
    _gameSpeed = (_difficultyIndex == 3) ? currentChaosSpeed() : DIFFICULTY_SPEEDS[_difficultyIndex];

    const unsigned long now = millis();
    if (now - _lastMoveTime < static_cast<unsigned long>(_gameSpeed)) {
        return;
    }

    _lastMoveTime = now;

    for (int i = _snakeLength - 1; i > 0; --i) {
        _snake[i] = _snake[i - 1];
    }

    _snake[0].x += _dirX;
    _snake[0].y += _dirY;

    if (_snake[0].x < GRID_MIN_X || _snake[0].x > GRID_MAX_X ||
        _snake[0].y < GRID_MIN_Y || _snake[0].y > GRID_MAX_Y) {
        gameOver();
        return;
    }

    for (int i = 1; i < _snakeLength; ++i) {
        if (_snake[0].x == _snake[i].x && _snake[0].y == _snake[i].y) {
            gameOver();
            return;
        }
    }

    if (_snake[0].x == _fruit.x && _snake[0].y == _fruit.y) {
        if (_snakeLength < MAX_SNAKE_LENGTH) {
            ++_snakeLength;
            _snake[_snakeLength - 1] = _snake[_snakeLength - 2];
        }

        ++_fruitsEaten;
        spawnFruit();

        digitalWrite(controls.BUZZER_PIN, HIGH);
        _buzzerEndTime = millis() + 50;
    }
}

void SnakeLib::drawMenu() {
    controls.mutexLed([&]() {
        controls.strip.setPixelColor(0, 0, 255, 255);
        controls.strip.show();
    });

    controls.mutexDisplay([&]() {
        controls.display.clearDisplay();
        controls.display.setTextSize(1);
        controls.display.setTextColor(SH110X_WHITE);

        controls.display.setCursor((SCREEN_WIDTH - 8 * 5) / 2, 0);
        controls.display.println("SNAKE");

        for (int i = 0; i < MENU_ITEM_COUNT; ++i) {
            const int y = 20 + i * 12;
            controls.display.setCursor(10, y);
            controls.display.print(i == _menuIndex ? "> " : "  ");
            controls.display.println(MENU_ITEMS[i]);
        }

        controls.display.display();
    });
}

void SnakeLib::drawSettings() {
    controls.mutexDisplay([&]() {
        controls.display.clearDisplay();
        controls.display.setTextSize(1);
        controls.display.setTextColor(SH110X_WHITE);

        controls.display.setCursor((SCREEN_WIDTH - 8 * 10) / 2, 0);
        controls.display.println("NASTAVENI");

        controls.display.setCursor(10, 20);
        controls.display.println("Obtiznost");

        controls.display.setCursor(10, 32);
        controls.display.print("> ");
        controls.display.println(DIFFICULTY_LEVELS[_difficultyIndex]);

        controls.display.setCursor(10, 56);
        controls.display.println("Touch = zpet");

        controls.display.display();
    });

    updateDifficultyLED();
}

void SnakeLib::drawGame() {
    controls.mutexDisplay([&]() {
        controls.display.clearDisplay();
        controls.display.setTextSize(1);
        controls.display.setTextColor(SH110X_WHITE);

        controls.display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);

        for (int i = 0; i < _snakeLength; ++i) {
            controls.display.fillRect(
                _snake[i].x * CELL_SIZE,
                _snake[i].y * CELL_SIZE,
                CELL_SIZE,
                CELL_SIZE,
                SH110X_WHITE
            );
        }

        controls.display.fillRect(
            _fruit.x * CELL_SIZE,
            _fruit.y * CELL_SIZE,
            CELL_SIZE,
            CELL_SIZE,
            SH110X_WHITE
        );

        controls.display.display();
    });
}

void SnakeLib::drawGameOver() {
    int score = calculateWeightedScore(_fruitsEaten, _difficultyIndex);

    controls.mutexDisplay([&]() {
        controls.display.clearDisplay();
        controls.display.setTextSize(1);
        controls.display.setTextColor(SH110X_WHITE);

        controls.display.setCursor((SCREEN_WIDTH - 8 * 9) / 2, 0);
        controls.display.println("KONEC HRY");

        controls.display.setCursor(10, 16);
        controls.display.print("Ovoce: ");
        controls.display.println(_fruitsEaten);

        controls.display.setCursor(10, 28);
        controls.display.print("Skore: ");
        controls.display.println(score);

        controls.display.setCursor(10, 40);
        controls.display.print("Level: ");
        controls.display.println(DIFFICULTY_LEVELS[_difficultyIndex]);

        controls.display.setCursor(10, 56);
        controls.display.println("Touch = menu");

        controls.display.display();
    });
}

void SnakeLib::handleButtonPress() {
    switch (_gameState) {
        case MENU:
            if (controls.leftPressed) {
                _menuIndex = (_menuIndex - 1 + MENU_ITEM_COUNT) % MENU_ITEM_COUNT;
                controls.leftPressed = false;
            }

            if (controls.rightPressed) {
                _menuIndex = (_menuIndex + 1) % MENU_ITEM_COUNT;
                controls.rightPressed = false;
            }
            break;

        case SETTINGS:
            if (controls.leftPressed) {
                _difficultyIndex = (_difficultyIndex + 1) % DIFFICULTY_COUNT;
                updateDifficultyLED();
                controls.leftPressed = false;
            }

            if (controls.rightPressed) {
                _difficultyIndex = (_difficultyIndex - 1 + DIFFICULTY_COUNT) % DIFFICULTY_COUNT;
                updateDifficultyLED();
                controls.rightPressed = false;
            }
            break;

        case GAME:
            if (controls.leftPressed) {
                const int oldX = _dirX;
                _dirX = _dirY;
                _dirY = -oldX;
                controls.leftPressed = false;
            }

            if (controls.rightPressed) {
                const int oldX = _dirX;
                _dirX = -_dirY;
                _dirY = oldX;
                controls.rightPressed = false;
            }
            break;

        case GAME_OVER:
            controls.leftPressed = false;
            controls.rightPressed = false;
            break;
    }
}

void SnakeLib::resetGame() {
    updateDifficultyLED();
    invalidatePendingResult();

    _snakeLength = 2;
    _dirX = 1;
    _dirY = 0;
    _fruitsEaten = 0;
    _gameSpeed = DIFFICULTY_SPEEDS[_difficultyIndex];
    _buzzerEndTime = 0;
    _gameOverBeepUntil = 0;

    _snake[0] = {
        random(GRID_MIN_X + 2, GRID_MAX_X - 1),
        random(GRID_MIN_Y + 2, GRID_MAX_Y - 1)
    };
    _snake[1] = {_snake[0].x - 1, _snake[0].y};

    spawnFruit();

    _gameState = GAME;
    _lastMoveTime = millis();
}

void SnakeLib::gameOver() {
    pendingResult.fruits = _fruitsEaten;
    pendingResult.score = calculateWeightedScore(_fruitsEaten, _difficultyIndex);
    pendingResult.difficultyIndex = static_cast<uint8_t>(_difficultyIndex);
    pendingResult.saveWindowOpen = true;

    _gameState = GAME_OVER;
    _gameOverBeepUntil = millis() + 300;
    _buzzerEndTime = 0;
    digitalWrite(controls.BUZZER_PIN, HIGH);
}

void SnakeLib::spawnFruit() {
    static constexpr int FRUIT_MARGIN_LOCAL = 3;

    const int fruitMinX = GRID_MIN_X + FRUIT_MARGIN_LOCAL;
    const int fruitMaxX = GRID_MAX_X - FRUIT_MARGIN_LOCAL;
    const int fruitMinY = GRID_MIN_Y + FRUIT_MARGIN_LOCAL;
    const int fruitMaxY = GRID_MAX_Y - FRUIT_MARGIN_LOCAL;

    for (int attempt = 0; attempt < 100; ++attempt) {
        const int x = random(fruitMinX, fruitMaxX + 1);
        const int y = random(fruitMinY, fruitMaxY + 1);

        if (!isCellOccupied(x, y)) {
            _fruit.x = x;
            _fruit.y = y;
            return;
        }
    }

    for (int y = fruitMinY; y <= fruitMaxY; ++y) {
        for (int x = fruitMinX; x <= fruitMaxX; ++x) {
            if (!isCellOccupied(x, y)) {
                _fruit.x = x;
                _fruit.y = y;
                return;
            }
        }
    }
}

bool SnakeLib::isCellOccupied(int x, int y) const {
    for (int i = 0; i < _snakeLength; ++i) {
        if (_snake[i].x == x && _snake[i].y == y) {
            return true;
        }
    }
    return false;
}

void SnakeLib::handleTouchConfirm() {
    switch (_gameState) {
        case MENU:
            if (_menuIndex == 0) {
                resetGame();
            } else if (_menuIndex == 1) {
                _gameState = SETTINGS;
            } else {
                invalidatePendingResult();
                _exit = true;
            }
            break;

        case SETTINGS:
            _gameSpeed = DIFFICULTY_SPEEDS[_difficultyIndex];
            _gameState = MENU;
            break;

        case GAME:
            break;

        case GAME_OVER:
            invalidatePendingResult();
            _gameState = MENU;
            _menuIndex = 0;
            _fruitsEaten = 0;
            break;
    }
}

bool SnakeLib::shouldExit() {
    return _exit;
}

bool SnakeLib::isRunning() {
    return running;
}

void SnakeLib::updateDifficultyLED() {
    controls.mutexLed([&]() {
        switch (_difficultyIndex) {
            case 0:
                controls.strip.setPixelColor(0, 0, 255, 0);
                break;
            case 1:
                controls.strip.setPixelColor(0, 255, 165, 0);
                break;
            case 2:
                controls.strip.setPixelColor(0, 255, 0, 0);
                break;
            case 3:
                controls.strip.setPixelColor(0, 128, 0, 255);
                break;
        }
        controls.strip.show();
    });
}

void SnakeLib::updateBuzzer() {
    const unsigned long now = millis();

    if (_gameOverBeepUntil != 0) {
        if (now >= _gameOverBeepUntil) {
            digitalWrite(controls.BUZZER_PIN, LOW);
            _gameOverBeepUntil = 0;
        }
        return;
    }

    if (_buzzerEndTime != 0 && now >= _buzzerEndTime) {
        digitalWrite(controls.BUZZER_PIN, LOW);
        _buzzerEndTime = 0;
    }
}

int SnakeLib::calculateWeightedScore(int fruits, int difficultyIndex) const {
    int multiplier = 1;
    switch (difficultyIndex) {
        case 0: multiplier = 1; break;
        case 1: multiplier = 2; break;
        case 2: multiplier = 3; break;
        case 3: multiplier = 5; break;
        default: break;
    }
    return fruits * multiplier;
}

int SnakeLib::currentChaosSpeed() const {
    const int chaosA = 3;
    const int chaosB = 7;
    const int chaosC = 12;

    if (_fruitsEaten < chaosA) return random(200, 251);
    if (_fruitsEaten < chaosB) return random(130, 181);
    if (_fruitsEaten < chaosC) return random(90, 111);
    return random(30, 61);
}

void SnakeLib::invalidatePendingResult() {
    pendingResult.fruits = 0;
    pendingResult.score = 0;
    pendingResult.difficultyIndex = 0;
    pendingResult.saveWindowOpen = false;
}

void SnakeLib::loadLeaderboard() {
    if (!lockData()) return;

    leaderboardCount = 0;

    if (!LittleFS.exists(LEADERBOARD_FILE)) {
        unlockData();
        return;
    }

    File file = LittleFS.open(LEADERBOARD_FILE, "r");
    if (!file) {
        unlockData();
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err || !doc.is<JsonArray>()) {
        unlockData();
        return;
    }

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        if (leaderboardCount >= MAX_LEADERBOARD) break;

        LeaderboardEntry &entry = leaderboard[leaderboardCount];
        const char* name = obj["name"] | "---";
        strncpy(entry.name, name, sizeof(entry.name) - 1);
        entry.name[sizeof(entry.name) - 1] = '\0';
        entry.fruits = obj["fruits"] | 0;
        entry.score = obj["score"] | 0;

        const char* difficulty = obj["difficulty"] | DIFFICULTY_LEVELS[0];
        entry.difficultyIndex = 0;
        for (int i = 0; i < DIFFICULTY_COUNT; ++i) {
            if (strcmp(difficulty, DIFFICULTY_LEVELS[i]) == 0) {
                entry.difficultyIndex = static_cast<uint8_t>(i);
                break;
            }
        }

        ++leaderboardCount;
    }

    sortLeaderboard();
    unlockData();
}

void SnakeLib::saveLeaderboard() {
    if (!lockData()) return;

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (uint8_t i = 0; i < leaderboardCount; ++i) {
        JsonObject obj = arr.add<JsonObject>();
        obj["name"] = leaderboard[i].name;
        obj["difficulty"] = DIFFICULTY_LEVELS[leaderboard[i].difficultyIndex];
        obj["fruits"] = leaderboard[i].fruits;
        obj["score"] = leaderboard[i].score;
    }

    File file = LittleFS.open(LEADERBOARD_FILE, "w");
    if (file) {
        serializeJson(doc, file);
        file.close();
    }

    unlockData();
}

void SnakeLib::clearLeaderboardInternal() {
    if (!lockData()) return;
    leaderboardCount = 0;
    memset(leaderboard, 0, sizeof(leaderboard));
    unlockData();
    saveLeaderboard();
}

void SnakeLib::sortLeaderboard() {
    std::sort(leaderboard, leaderboard + leaderboardCount,
        [](const LeaderboardEntry& a, const LeaderboardEntry& b) {
            if (a.score != b.score) return a.score > b.score;
            return a.fruits > b.fruits;
        }
    );
}

bool SnakeLib::insertLeaderboardEntry(const char* name) {
    if (!lockData()) return false;

    if (_gameState != GAME_OVER || !pendingResult.saveWindowOpen) {
        unlockData();
        return false;
    }

    LeaderboardEntry entry = {};
    strncpy(entry.name, (name != nullptr && name[0] != '\0') ? name : "---", sizeof(entry.name) - 1);
    entry.name[sizeof(entry.name) - 1] = '\0';
    entry.fruits = pendingResult.fruits;
    entry.score = pendingResult.score;
    entry.difficultyIndex = pendingResult.difficultyIndex;

    bool inserted = false;

    if (leaderboardCount < MAX_LEADERBOARD) {
        leaderboard[leaderboardCount++] = entry;
        inserted = true;
    } else {
        sortLeaderboard();
        const LeaderboardEntry& worst = leaderboard[leaderboardCount - 1];
        if (entry.score > worst.score || (entry.score == worst.score && entry.fruits > worst.fruits)) {
            leaderboard[leaderboardCount - 1] = entry;
            inserted = true;
        }
    }

    if (inserted) {
        sortLeaderboard();
    }

    pendingResult.saveWindowOpen = false;
    unlockData();

    if (inserted) {
        saveLeaderboard();
    }

    return inserted;
}

void SnakeLib::registerWebRoutes() {
    settings.server().on("/snake/setDifficulty", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("level")) {
            setDifficulty(request->getParam("level")->value().toInt());
        }
        request->send(200, "text/plain", "Difficulty set");
    });

    settings.server().on("/snake/setName", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String name = request->hasParam("name") ? request->getParam("name")->value() : "";
        bool accepted = insertLeaderboardEntry(name.c_str());
        request->send(accepted ? 200 : 409, "text/plain", accepted ? "OK" : "Result unavailable");
    });

    settings.server().on("/snake/leaderboard", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!lockData()) {
            request->send(500, "application/json", "[]");
            return;
        }

        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        for (uint8_t i = 0; i < leaderboardCount; ++i) {
            JsonObject obj = arr.add<JsonObject>();
            obj["name"] = leaderboard[i].name;
            obj["difficulty"] = DIFFICULTY_LEVELS[leaderboard[i].difficultyIndex];
            obj["fruits"] = leaderboard[i].fruits;
            obj["score"] = leaderboard[i].score;
        }

        String json;
        serializeJson(doc, json);
        unlockData();

        request->send(200, "application/json", json);
    });

    settings.server().on("/snake/exit", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (lockData()) {
            invalidatePendingResult();
            _exit = true;
            unlockData();
        }
        request->send(200, "text/plain", "ok");
    });

    settings.server().on("/snake/clearLeaderboard", HTTP_GET, [this](AsyncWebServerRequest *request) {
        clearLeaderboardInternal();
        request->send(200, "text/plain", "OK");
    });
}

void SnakeLib::setDifficulty(int level) {
    if (level < 0 || level >= DIFFICULTY_COUNT) return;

    if (lockData()) {
        _difficultyIndex = level;
        _gameSpeed = DIFFICULTY_SPEEDS[_difficultyIndex];
        unlockData();
    }

    updateDifficultyLED();
}