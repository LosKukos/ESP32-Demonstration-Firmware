#ifndef SNAKE_LIB_H
#define SNAKE_LIB_H

#include <Wire.h>
#include <controls.h>
#include <web.h>
#include <vector>

class SnakeLib {
public:

    SnakeLib(Controls& ctrl); // Konstruktor knihovny hada

    // Pomocné funkce pro menu a nastavení
    void begin(); // Volá se při vstupu do hry
    void loop(); // Smyčka hry
    bool shouldExit(); // Vrácení stavu pro návrat do menu
    void handleButtonPress(); // Zpracování stisků tlačítek
    void handleTouchConfirm(); // Zpracování potvrzení dotyku
    void updateDifficultyLED(); // Aktualizace LED indikace obtížnosti
    void web_SnakeLib(); // Webová stránka pro Snake
    void setDifficulty(int level); // Nastavení obtížnosti z webu

    struct LeaderboardEntry { // Struktura pro záznam v leaderboardu
        String name; // Jméno hráče
        String difficulty; // Obtížnost, na které byla hra odehrána
        int score; // Výsledné skóre
    };
	
private:
    Controls& controls; // Reference na ovládací prvky (tlačítka, display, LED)

    // Stav hry
    enum GameState { MENU, SETTINGS, GAME, GAME_OVER }; // Hlavní stavy hry
    GameState _gameState; // Aktuální stav hry

    // Menu 
    int _menuIndex; // Index aktuálně vybrané položky v menu
    int _menuItemCount; // Počet položek v menu
    String _menuItems[3]; // Položky menu

    // Nastavení obtížnosti
    String _difficultyLevels[4]; // Popisky obtížností
    int _difficultySpeeds[4]; // Rychlosti pro jednotlivé obtížnosti
    int _gameSpeed; // Aktuální rychlost hry (dle obtížnosti)
    int _difficultyIndex; // Index aktuálně vybrané obtížnosti

    // Had a ovoce
    struct Point { int x, y; }; // Struktura pro reprezentaci bodu na herní ploše
    int _snakeLength; // Aktuální délka hada
    int _dirX, _dirY; // Směr pohybu hada (1, 0, -1)
    Point _snake[256]; // Pole pro uložení pozic hada (max délka 256)
    Point _fruit; // Pozice ovoce
    int _fruitsEaten; // Počet snězeného ovoce

    // Časování pohybu hada
    unsigned long _lastMoveTime; // Proměnná pro uložení času posledního pohybu hada

    // Pomocné funkce
    void resetGame(); // Reset hry do výchozího stavu
    void gameOver(); // Zpracování konce hry
    void spawnFruit(); // Generování nové pozice ovoce

    // Ukončení
	bool _exit;  // Indikátor pro opuštění menu

    // Webová stránka pro Snake
    std::vector<LeaderboardEntry> leaderboard; // Vektor pro uložení záznamů v leaderboardu
};

#endif
