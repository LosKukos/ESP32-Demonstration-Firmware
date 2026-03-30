#ifndef SNAKE_LIB_H
#define SNAKE_LIB_H

#include <Wire.h>
#include <controls.h>
#include <web.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

class SnakeLib {
public:
    SnakeLib(Controls& ctrl);

    void begin();
    bool shouldExit();
    bool isRunning();

    TaskHandle_t getTaskHandle() const { return taskHandle; }
    void clearTaskHandle() { taskHandle = nullptr; }
    void requestStop() { _exit = true; }

    void web_SnakeLib();
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

    static constexpr int MENU_ITEM_COUNT = 3;
    static constexpr int DIFFICULTY_COUNT = 4;
    static constexpr int CELL_SIZE = 4;
    static constexpr int GRID_MIN_X = 1;
    static constexpr int GRID_MIN_Y = 1;
    static constexpr int GRID_MAX_X = (SCREEN_WIDTH / CELL_SIZE) - 2;
    static constexpr int GRID_MAX_Y = (SCREEN_HEIGHT / CELL_SIZE) - 2;
    static constexpr int GRID_WIDTH = GRID_MAX_X - GRID_MIN_X + 1;
    static constexpr int GRID_HEIGHT = GRID_MAX_Y - GRID_MIN_Y + 1;
    static constexpr int MAX_SNAKE_LENGTH = GRID_WIDTH * GRID_HEIGHT;
    static constexpr int FRUIT_MARGIN = 3;
    static constexpr int MAX_LEADERBOARD = 10;
    static constexpr const char* LEADERBOARD_FILE = "/leaderboard.json";

    static const char* const MENU_ITEMS[MENU_ITEM_COUNT];
    static const char* const DIFFICULTY_LEVELS[DIFFICULTY_COUNT];
    static const int DIFFICULTY_SPEEDS[DIFFICULTY_COUNT];

    TaskHandle_t taskHandle = nullptr;
    SemaphoreHandle_t dataMutex = nullptr;

    GameState _gameState = MENU;

    int _menuIndex = 0;
    int _difficultyIndex = 1;
    int _gameSpeed = DIFFICULTY_SPEEDS[1];

    int _snakeLength = 2;
    int _dirX = 1;
    int _dirY = 0;
    Point _snake[MAX_SNAKE_LENGTH] = {};
    Point _fruit = {GRID_MIN_X, GRID_MIN_Y};
    int _fruitsEaten = 0;

    unsigned long _lastMoveTime = 0;
    unsigned long _buzzerEndTime = 0;
    unsigned long _gameOverBeepUntil = 0;

    volatile bool _exit = false;

    PendingResult pendingResult = {0, 0, 0, false};
    LeaderboardEntry leaderboard[MAX_LEADERBOARD] = {};
    uint8_t leaderboardCount = 0;

    static void task(void *pvParameters);
    bool running = false;

    void runGameLogic();
    void renderMenu();
    void renderSettings();
    void renderGame();
    void renderGameOver();

    void handleButtonPress();
    void handleTouchConfirm();
    void updateDifficultyLED();
    void updateBuzzer();

    void resetGame();
    void gameOver();
    void spawnFruit();
    bool isCellOccupied(int x, int y) const;
    int calculateWeightedScore(int fruits, int difficultyIndex) const;
    int currentChaosSpeed() const;
    void invalidatePendingResult();

    void loadLeaderboard();
    void saveLeaderboard();
    void clearLeaderboardInternal();
    bool insertLeaderboardEntry(const char* name);
    void sortLeaderboard();
    void clearSnakeRoutes();
    bool lockData(TickType_t ticks = portMAX_DELAY);
    void unlockData();
};

#endif
