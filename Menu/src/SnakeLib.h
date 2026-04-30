#pragma once

#include <Wire.h>
#include <controls.h>
#include <settings.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

class SnakeLib {
public:
    SnakeLib(Controls& ctrl, Settings& settingsRef);

    // Inicializace modulu
    void begin();

    // Stav modulu
    bool shouldExit();
    bool isRunning();
    void requestStop() { _exit = true; }

    // Webové rozhraní
    void registerWebRoutes();

    // Nastavení obtížnosti
    void setDifficulty(int level);

private:
    struct Point {
        int x;
        int y;
    };

    enum GameState {
        MENU,
        SETTINGS,
        GAME,
        GAME_OVER
    };

    struct LeaderboardEntry {
        char name[17];
        uint16_t fruits;
        uint16_t score;
        uint8_t difficultyIndex;
    };

    struct PendingResult {
        uint16_t fruits;
        uint16_t score;
        uint8_t difficultyIndex;
        bool saveWindowOpen;
    };

    Controls& controls;
    Settings& settings;

    // Nastavení menu a obtížnosti
    static constexpr int MENU_ITEM_COUNT = 3;
    static constexpr int DIFFICULTY_COUNT = 4;

    // Nastavení herní plochy
    static constexpr int CELL_SIZE = 4;
    static constexpr int GRID_MIN_X = 1;
    static constexpr int GRID_MIN_Y = 1;
    static constexpr int GRID_MAX_X = (SCREEN_WIDTH / CELL_SIZE) - 2;
    static constexpr int GRID_MAX_Y = (SCREEN_HEIGHT / CELL_SIZE) - 2;
    static constexpr int GRID_WIDTH = GRID_MAX_X - GRID_MIN_X + 1;
    static constexpr int GRID_HEIGHT = GRID_MAX_Y - GRID_MIN_Y + 1;
    static constexpr int MAX_SNAKE_LENGTH = GRID_WIDTH * GRID_HEIGHT;

    // Nastavení tabulky výsledků
    static constexpr int MAX_LEADERBOARD = 10;
    static constexpr const char* LEADERBOARD_FILE = "/leaderboard.json";

    static const char* const MENU_ITEMS[MENU_ITEM_COUNT];
    static const char* const DIFFICULTY_LEVELS[DIFFICULTY_COUNT];
    static const int DIFFICULTY_SPEEDS[DIFFICULTY_COUNT];

    // FreeRTOS task modulu
    TaskHandle_t taskHandle = nullptr;
    static void task(void *pvParameters);

    // Sdílená data modulu
    SemaphoreHandle_t dataMutex = nullptr;
    bool running = false;

    // Stav hry
    GameState _gameState = MENU;
    int _menuIndex = 0;
    int _difficultyIndex = 1;
    int _gameSpeed = DIFFICULTY_SPEEDS[1];

    // Herní data
    int _snakeLength = 2;
    int _dirX = 1;
    int _dirY = 0;
    Point _snake[MAX_SNAKE_LENGTH] = {};
    Point _fruit = {GRID_MIN_X, GRID_MIN_Y};
    int _fruitsEaten = 0;

    // Časování hry a bzučáku
    unsigned long _lastMoveTime = 0;
    unsigned long _buzzerEndTime = 0;
    unsigned long _gameOverBeepUntil = 0;

    volatile bool _exit = false;

    // Data pro ukládání výsledků
    PendingResult pendingResult = {0, 0, 0, false};
    LeaderboardEntry leaderboard[MAX_LEADERBOARD] = {};
    uint8_t leaderboardCount = 0;

    // Herní logika
    void runGameLogic();
    void resetGame();
    void gameOver();
    void spawnFruit();
    bool isCellOccupied(int x, int y) const;
    int calculateWeightedScore(int fruits, int difficultyIndex) const;
    int currentChaosSpeed() const;

    // Vykreslení obrazovek
    void drawMenu();
    void drawSettings();
    void drawGame();
    void drawGameOver();

    // Zpracování vstupů
    void handleButtonPress();
    void handleTouchConfirm();

    // Ovládání LED a bzučáku
    void updateDifficultyLED();
    void updateBuzzer();

    // Práce s výsledky
    void invalidatePendingResult();
    void loadLeaderboard();
    void saveLeaderboard();
    void clearLeaderboardInternal();
    bool insertLeaderboardEntry(const char* name);
    void sortLeaderboard();

    // Uzamčení sdílených dat
    bool lockData(TickType_t ticks = portMAX_DELAY);
    void unlockData();
};
