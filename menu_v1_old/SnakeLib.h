#ifndef SNAKE_LIB_H
#define SNAKE_LIB_H

#include <Wire.h>
#include <controls.h>

class SnakeLib {
public:

    SnakeLib(Controls& ctrl);

    // --- Pomocné funkce ---
    void begin();
    void loop();
    void exit();  // nastavení flagu pro návrat
    bool shouldExit();  // vrácení stavu pro návrat
    void handleButtonPress();
    void handleTouchConfirm();
    void updateDifficultyLED();
	
private:
    Controls& controls;

    // --- Stav hry ---
    enum GameState { MENU, SETTINGS, GAME, GAME_OVER };
    GameState _gameState;

    // --- Menu ---
    int _menuIndex;
    int _menuItemCount;
    String _menuItems[3];

    // --- Nastavení obtížnosti ---
    String _difficultyLevels[3];
    int _difficultySpeeds[3];
    int _gameSpeed;
    int _difficultyIndex;

    // --- Had a ovoce ---
    struct Point { int x, y; };
    int _snakeLength;
    int _dirX, _dirY;
    Point _snake[256];
    Point _fruit;

    // --- Časování pohybu hada ---
    unsigned long _lastMoveTime;

    // --- Pomocné funkce ---
    void resetGame();
    void gameOver();
    void spawnFruit();

    // --- Instance pro ISR ---
    static SnakeLib* _instance;

    // --- Ukončení ---
	bool _exit;  // flag pro ukončení
};

#endif
