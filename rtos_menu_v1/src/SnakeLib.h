#ifndef SNAKE_LIB_H
#define SNAKE_LIB_H

#include <Wire.h>
#include <controls.h>
#include <web.h>

#include <vector>
#include <LittleFS.h>
#include <ArduinoJson.h>

struct LeaderboardEntry {
    String name;
    int score;
    String difficulty;
    int weightedScore;
};

class SnakeLib {
public:

    SnakeLib(Controls& ctrl);

    void begin();
    bool shouldExit();
    bool isRunning();

    // Web
    void web_SnakeLib();
    void setDifficulty(int level);

private:
    Controls& controls;

    // Task
    static void task(void *pvParameters);
    TaskHandle_t taskHandle;

    // State machine
    enum GameState { MENU, SETTINGS, GAME, GAME_OVER };
    GameState _gameState;

    // Menu
    int _menuIndex;
    int _menuItemCount;
    String _menuItems[3];

    // Difficulty
    String _difficultyLevels[4];
    int _difficultySpeeds[4];
    int _gameSpeed;
    int _difficultyIndex;

    // Snake
    struct Point { int x, y; };
    int _snakeLength;
    int _dirX, _dirY;
    Point _snake[256];
    Point _fruit;
    int _fruitsEaten;

    // Timing
    unsigned long _lastMoveTime;
    unsigned long _buzzerEndTime;

    // Exit flag
    volatile bool _exit;

    // Leaderboard
    std::vector<LeaderboardEntry> leaderboard;

    // Internal logic
    void runGameLogic();
    void renderMenu();
    void renderSettings();
    void renderGame();
    void renderGameOver();

    void handleButtonPress();
    void handleTouchConfirm();
    void updateDifficultyLED();

    void resetGame();
    void gameOver();
    void spawnFruit();

    void loadLeaderboard();
    void saveLeaderboard();
};

#endif