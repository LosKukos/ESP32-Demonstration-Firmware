#include "SnakeLib.h"

SnakeLib::SnakeLib(Controls& ctrl) : controls(ctrl) {

    _gameState = MENU;
    _menuIndex = 0;
    _menuItemCount = 3;

    _menuItems[0] = "Start";
    _menuItems[1] = "Nastaveni";
    _menuItems[2] = "Konec";

    _difficultyIndex = 1;

    _difficultyLevels[0] = "Lehka";
    _difficultyLevels[1] = "Stredni";
    _difficultyLevels[2] = "Tezka";
    _difficultyLevels[3] = "Chaos";

    _difficultySpeeds[0] = 200;
    _difficultySpeeds[1] = 150;
    _difficultySpeeds[2] = 100;
    _difficultySpeeds[3] = 200;

    _gameSpeed = _difficultySpeeds[_difficultyIndex];

    _exit = false;
    _fruitsEaten = 0;

    loadLeaderboard();
}

int getDifficultyMultiplier(const String& diff) {
    if (diff == "Lehka") return 1;
    if (diff == "Stredni") return 2;
    if (diff == "Tezka") return 3;
    if (diff == "Chaos") return 5;
    return 1;
}

void SnakeLib::loadLeaderboard() {
    leaderboard.clear();

    if (!LittleFS.exists("/leaderboard.json")) return;

    File file = LittleFS.open("/leaderboard.json", "r");
    if (!file) return;

    String content = file.readString();
    file.close();

    JsonDocument doc;

    DeserializationError err = deserializeJson(doc, content);
    if (err) return;

    JsonArray arr = doc.as<JsonArray>();

    for (JsonObject obj : arr) {
        LeaderboardEntry entry;
        entry.name = obj["name"] | "---";
        entry.score = obj["score"] | 0;
        entry.difficulty = obj["difficulty"] | "Lehka";
        entry.weightedScore = obj["weightedScore"] | 0;
        leaderboard.push_back(entry);
    }
}

void SnakeLib::saveLeaderboard() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (auto &entry : leaderboard) {
        JsonObject obj = arr.add<JsonObject>();
        obj["name"] = entry.name;
        obj["score"] = entry.score;
        obj["difficulty"] = entry.difficulty;
        obj["weightedScore"] = entry.weightedScore;
    }

    File file = LittleFS.open("/leaderboard.json", "w");
    if (!file) return;

    serializeJson(doc, file);
    file.close();
}

void SnakeLib::begin() {

    _snake[0] = {
        random(3, 128/8 - 3),
        random(3, 64/8 - 3)
    };

    _snakeLength = 2;
    _dirX = 1;
    _dirY = 0;
    _lastMoveTime = millis();
    _exit = false;
    _menuIndex = 0;

    xTaskCreatePinnedToCore(
        task,
        "SnakeTask",
        4096,
        this,
        1,
        &taskHandle,
        1
    );
}

void SnakeLib::task(void *pvParameters) {

    SnakeLib* self = static_cast<SnakeLib*>(pvParameters);

    Serial.println("[SnakeTask] Started");

    for (;;) {

        if (self->_exit) {
            Serial.println("[SnakeTask] Exit flag detected");
            break;
        }

        if (self->controls.FingerTouchedFlag()) {
            self->handleTouchConfirm();
        }

        if (self->controls.leftPressed || self->controls.rightPressed) {
            self->handleButtonPress();
        }

        switch (self->_gameState) {

            case MENU:
                self->renderMenu();
                break;

            case SETTINGS:
                self->renderSettings();
                break;

            case GAME:
                self->runGameLogic();
                self->renderGame();
                break;

            case GAME_OVER:
                self->renderGameOver();
                break;
        }

        vTaskDelay(20 / portTICK_PERIOD_MS);
    }

    Serial.println("[SnakeTask] Exiting loop");

    self->taskHandle = NULL;

    Serial.println("[SnakeTask] Handle cleared, deleting task");

    vTaskDelete(NULL);
}

void SnakeLib::runGameLogic() {

    if (_difficultyIndex == 3) {
        int Chaos_a = random(1,4);
        int Chaos_b = random(Chaos_a,8);
        int Chaos_c = random(Chaos_b, 13);
        if (_fruitsEaten < Chaos_a) _gameSpeed = random(200, 251);
        else if (_fruitsEaten < Chaos_b) _gameSpeed = random(130, 181);
        else if (_fruitsEaten < Chaos_c) _gameSpeed = random(90, 111);
        else _gameSpeed = random(30, 61);
    } else {
        _gameSpeed = _difficultySpeeds[_difficultyIndex];
    }

    if (millis() - _lastMoveTime >= _gameSpeed) {
        _lastMoveTime = millis();

        for (int i = _snakeLength - 1; i > 0; i--)
            _snake[i] = _snake[i - 1];

        _snake[0].x += _dirX;
        _snake[0].y += _dirY;

        if (_snake[0].x < 1 || _snake[0].x >= 128/4 - 1 ||
            _snake[0].y < 1 || _snake[0].y >= 64/4 - 1) {
            gameOver();
            return;
        }

        for (int i = 1; i < _snakeLength; i++) {
            if (_snake[0].x == _snake[i].x && _snake[0].y == _snake[i].y) {
                gameOver();
                return;
            }
        }

        if (_snake[0].x == _fruit.x && _snake[0].y == _fruit.y) {
            _snakeLength++;
            _fruitsEaten++;
            spawnFruit();

            digitalWrite(controls.BUZZER_PIN, HIGH);
            _buzzerEndTime = millis() + 50;
        }

        if (_buzzerEndTime && millis() >= _buzzerEndTime) {
            digitalWrite(controls.BUZZER_PIN, LOW);
            _buzzerEndTime = 0;
        }
    }
}

void SnakeLib::renderMenu() {

    // LED
    controls.mutexLed([&]() {
        controls.strip.setPixelColor(0, 0, 255, 255);
        controls.strip.show();
    });

    // DISPLAY
    controls.mutexDisplay([&]() {

        controls.display.clearDisplay();
        controls.display.setTextSize(1);
        controls.display.setTextColor(SH110X_WHITE);

        controls.display.setCursor(28, 5);
        controls.display.println("MENU SNAKE");

        for (int i = 0; i < _menuItemCount; i++) {
            int y = 25 + i * 12;
            controls.display.setCursor(30, y);

            if (i == _menuIndex) controls.display.print("> ");
            else controls.display.print("  ");

            controls.display.println(_menuItems[i]);
        }

        controls.display.display();
    });
}

void SnakeLib::renderSettings() {

    controls.mutexDisplay([&]() {

        controls.display.clearDisplay();
        controls.display.setTextSize(1);
        controls.display.setTextColor(SH110X_WHITE);

        controls.display.setCursor(25, 10);
        controls.display.println("Obtiznost:");

        controls.display.setCursor(40, 25);
        controls.display.print("> ");
        controls.display.println(_difficultyLevels[_difficultyIndex]);

        controls.display.display();
    });

    updateDifficultyLED();
}

void SnakeLib::renderGame() {

    controls.mutexDisplay([&]() {

        controls.display.clearDisplay();
        controls.display.setTextSize(1);
        controls.display.setTextColor(SH110X_WHITE);

        controls.display.drawRect(0, 0, 128, 64, SH110X_WHITE);

        for (int i = 0; i < _snakeLength; i++)
            controls.display.fillRect(
                _snake[i].x * 4,
                _snake[i].y * 4,
                4,
                4,
                SH110X_WHITE
            );

        controls.display.fillRect(
            _fruit.x * 4,
            _fruit.y * 4,
            4,
            4,
            SH110X_WHITE
        );

        controls.display.display();
    });
}

void SnakeLib::renderGameOver() {

    controls.mutexDisplay([&]() {

        controls.display.clearDisplay();
        controls.display.setTextSize(1);
        controls.display.setTextColor(SH110X_WHITE);

        controls.display.setCursor(30, 0);
        controls.display.println("KONEC HRY");

        controls.display.setCursor(0, 12);
        controls.display.print("Ovoce: ");
        controls.display.println(_fruitsEaten);

        controls.display.setCursor(0, 22);
        controls.display.print("Obtiznost: ");
        controls.display.println(_difficultyLevels[_difficultyIndex]);

        controls.display.setCursor(0, 32);
        controls.display.println("Web: zadejte jmeno");

        controls.display.setCursor(0, 52);
        controls.display.println("Dotkni se pro navrat");

        controls.display.display();
    });
}

void SnakeLib::handleButtonPress() {

    switch (_gameState) {

        case MENU:
            if (controls.leftPressed) {
                _menuIndex = (_menuIndex - 1 + _menuItemCount) % _menuItemCount;
                controls.leftPressed = false;
            }

            if (controls.rightPressed) {
                _menuIndex = (_menuIndex + 1) % _menuItemCount;
                controls.rightPressed = false;
            }
            break;

        case SETTINGS:
            if (controls.leftPressed) {
                _difficultyIndex = (_difficultyIndex + 1) % 4;
                updateDifficultyLED();
                controls.leftPressed = false;
            }

            if (controls.rightPressed) {
                _difficultyIndex = (_difficultyIndex - 1 + 4) % 4;
                updateDifficultyLED();
                controls.rightPressed = false;
            }
            break;

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

void SnakeLib::resetGame() {

    updateDifficultyLED();

    _snake[0] = {
        random(3, 128/8 - 3),
        random(3, 64/8 - 3)
    };

    _snakeLength = 2;
    _dirX = 1;
    _dirY = 0;
    _fruitsEaten = 0;

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
        _fruit.x = random(3, 128/4 - 3);
        _fruit.y = random(3, 64/4 - 3);

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
            if (_menuIndex == 0) resetGame();
            else if (_menuIndex == 1) _gameState = SETTINGS;
            else if (_menuIndex == 2) _exit = true;
            break;

        case SETTINGS:
            _gameSpeed = _difficultySpeeds[_difficultyIndex];
            _gameState = MENU;
            break;

        case GAME_OVER:
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
    return taskHandle != NULL;
}

void SnakeLib::updateDifficultyLED() {

    controls.mutexLed([&]() {

        switch (_difficultyIndex) {
            case 0: controls.strip.setPixelColor(0, 0, 255, 0); break;
            case 1: controls.strip.setPixelColor(0, 255, 165, 0); break;
            case 2: controls.strip.setPixelColor(0, 255, 0, 0); break;
            case 3: controls.strip.setPixelColor(0, 128, 0, 255); break;
        }

        controls.strip.show();
    });
}

void SnakeLib::web_SnakeLib() {

    web.server().on("/snake/setDifficulty", HTTP_GET, [this](AsyncWebServerRequest *request) {

        if (request->hasParam("level")) {
            int level = request->getParam("level")->value().toInt();
            setDifficulty(level);
        }

        request->send(200, "text/plain", "Difficulty set");
    });

    web.server().on("/snake/setName", HTTP_GET, [this](AsyncWebServerRequest *request) {

        if (request->hasParam("name")) {
            String name = request->getParam("name")->value();

            LeaderboardEntry entry;
            entry.name = name.length() ? name : "---";
            entry.score = _fruitsEaten;
            entry.difficulty = _difficultyLevels[_difficultyIndex];

            // výpočet váženého skóre
            int multiplier = getDifficultyMultiplier(entry.difficulty);
            entry.weightedScore = entry.score * multiplier;

            leaderboard.push_back(entry);
            saveLeaderboard();
        }

        request->send(200, "text/plain", "OK");
    });

    web.server().on("/snake/leaderboard", HTTP_GET, [this](AsyncWebServerRequest *request) {

        loadLeaderboard();

        std::sort(leaderboard.begin(), leaderboard.end(),
            [](const LeaderboardEntry &a, const LeaderboardEntry &b) {
                return a.weightedScore > b.weightedScore;
            });

        String json = "[";
            for (size_t i = 0; i < leaderboard.size(); i++) {
                json += "{";
                json += "\"name\":\"" + leaderboard[i].name + "\",";
                json += "\"score\":" + String(leaderboard[i].score) + ",";
                json += "\"difficulty\":\"" + leaderboard[i].difficulty + "\",";
                json += "\"weightedScore\":" + String(leaderboard[i].weightedScore);
                json += "}";

                if (i < leaderboard.size() - 1) json += ",";
            }
        json += "]";

        request->send(200, "application/json", json);
    });

    web.server().on("/snake/exit", HTTP_GET, [this](AsyncWebServerRequest *request) {
        _exit = true;
        request->send(200, "text/plain", "ok");
    });

    web.server().on("/snake/clearLeaderboard", HTTP_GET, [](AsyncWebServerRequest *request){
        File file = LittleFS.open("/leaderboard.json", "w");
        file.print("[]");
        file.close();

        request->send(200, "text/plain", "OK");
    });
}

void SnakeLib::setDifficulty(int level){
    if(level < 0 || level > 3) return;

    _difficultyIndex = level;
    _gameSpeed = _difficultySpeeds[_difficultyIndex];

    updateDifficultyLED();
}