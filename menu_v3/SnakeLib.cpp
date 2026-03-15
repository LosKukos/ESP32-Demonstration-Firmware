#include "SnakeLib.h"

SnakeLib::SnakeLib(Controls& ctrl) : controls(ctrl) {
        // Nastavení polohy v menu, počtu položek a názvů v menu
        _gameState = MENU; // Výchozí stav je menu
        _menuIndex = 0; // Výchozí index menu
        _menuItemCount = 3; // Počet položek v menu
        _menuItems[0] = "Start"; // Položka pro spuštění hry
        _menuItems[1] = "Nastaveni"; // Položka pro nastavení obtížnosti
        _menuItems[2] = "Konec"; // Položka pro ukončení hry

        // Nastavení obtížností - jejich názvů a rychlosti
        _difficultyIndex = 1; // Výchozí obtížnost (střední)
        _difficultyLevels[0] = "Lehka"; // Název pro lehkou obtížnost
        _difficultyLevels[1] = "Stredni"; // Název pro střední obtížnost
        _difficultyLevels[2] = "Tezka"; // Název pro těžkou obtížnost
        _difficultyLevels[3] = "Chaos"; // Název pro chaotickou obtížnost
        _difficultySpeeds[0] = 200; // Rychlost pro lehkou obtížnost
        _difficultySpeeds[1] = 150; // Rychlost pro střední obtížnost
        _difficultySpeeds[2] = 100; // Rychlost pro těžkou obtížnost
        _difficultySpeeds[3] = 200; // Základní rychlost pro chaotickou obtížnost (bude se dynamicky měnit)
        _gameSpeed = _difficultySpeeds[_difficultyIndex]; // Nastavení počáteční rychlosti hry podle výchozí obtížnosti

        _exit = false; // Inicializace proměnné pro ukončení hry
        _fruitsEaten = 0; // Inicializace počtu snězeného ovoce
    }

void SnakeLib::begin() {
        // Inicializace pinu bzučáku na výstup
        pinMode(controls.BUZZER_PIN, OUTPUT);
        // Inicializace pozice hada
        _snake[0] = {
            random(3, 128/8 - 3),  // x: náhodná pozice hada na ose X, odstup od okrajů mapy
            random(3, 64/8 - 3)    // y: náhodná pozice hada na ose Y, odstup od okrajů mapy
        };
        _snakeLength = 2;   // Počáteční délka hada
        _dirX = 1; // Směr v ose X
        _dirY = 0; // Směr v ose Y
        _lastMoveTime = millis(); // Proměnná pro uložení času posledního pohybu
        _exit = false; // Proměnná pro odchod ze hry
        _menuIndex = 0; // Reset indexu menu
}

void SnakeLib::loop() {
    // Ukončení hry
        if (_exit) {
            controls.strip.setPixelColor(0, 0, 0, 0); // zhasnutí LED při odchodu
            controls.strip.show(); // aktualizace stavu LED
            return;
        }

    // Zpracování dotyku
        if (controls.FingerTouchedFlag()) { // Kontrola, zda byl detekován dotyk
            handleTouchConfirm(); // Zpracování potvrzení dotyku (např. výběr v menu, potvrzení volby)
        }

    // Zpracování tlačítek 
        if (controls.leftPressed) { // Kontrola, zda bylo stisknuto levé tlačítko
            handleButtonPress(); // Zpracování stisků tlačítek (např. pohyb v menu, změna směru hada)
        }
        if (controls.rightPressed) { // Kontrola, zda bylo stisknuto pravé tlačítko
            handleButtonPress(); // Zpracování stisků tlačítek (např. pohyb v menu, změna směru hada)
        }

    // Hlavní stavový automat
    switch (_gameState) {
        // Hlavní menu - vykreslení jednotlivých položek menu
            case MENU: // Vykreslení menu na OLED displej
            controls.strip.setPixelColor(0, 0, 255, 255); // modrá LED indikace pro menu
            controls.strip.show(); // aktualizace stavu LED
            controls.display.clearDisplay(); // Vyčištění displeje před vykreslením menu
            controls.display.setTextSize(1); // Nastavení velikosti textu pro menu
            controls.display.setTextColor(SH110X_WHITE); // Nastavení barvy textu pro menu
            controls.display.setCursor(28, 5); // Nastavení pozice kurzoru pro nadpis menu
            controls.display.println("MENU SNAKE"); // Vykreslení nadpisu menu
            for (int i = 0; i < _menuItemCount; i++) { // Smyčka pro vykreslení jednotlivých položek menu
                int y = 25 + i * 12; // Rovnoměrné odsazení položek menu od sebe
                controls.display.setCursor(30, y); // Nastavení pozice kurzoru pro položky menu
                if (i == _menuIndex) controls.display.print("> "); // Indikace aktuálně vybrané položky menu
                else controls.display.print("  "); // Mezery pro nevybranou položku
                controls.display.println(_menuItems[i]); // Vykreslení názvu položky menu
            }
            controls.display.display(); // Aktualizace displeje pro zobrazení menu
            break;

        // Nastavení hry - vykreslení menu nastavení a možnost výběru obtížnosti
            case SETTINGS: // Vykreslení nastavení na OLED displej
            controls.display.clearDisplay(); // Vyčištění displeje před vykreslením nastavení
            controls.display.setTextSize(1); // Nastavení velikosti textu pro nastavení
            controls.display.setTextColor(SH110X_WHITE); // Nastavení barvy textu pro nastavení
            controls.display.setCursor(25, 10); // Nastavení pozice kurzoru pro nadpis nastavení
            controls.display.println("Obtiznost:"); // Vykreslení nadpisu pro nastavení obtížnosti
            controls.display.setCursor(40, 25); // Nastavení pozice kurzoru pro zobrazení aktuální obtížnosti
            controls.display.print("> "); // Indikace pro zobrazení, že se jedná o nastavení obtížnosti
            controls.display.println(_difficultyLevels[_difficultyIndex]); // Vykreslení aktuální obtížnosti
            controls.display.display(); // Aktualizace displeje pro zobrazení nastavení
            updateDifficultyLED(); // Aktualizace LED indikace pro nastavení obtížnosti
            break;

        // Samotná hra
            case GAME: 
                // Dynamická rychlost pro chaotickou obtížnost - čím více ovoce had sní, tím rychlejší se stává
                if (_difficultyIndex == 3) { // Chaos
                    if (_fruitsEaten < 3) _gameSpeed = random(200, 251); // Velmi pomalá rychlost pro začátek
                    else if (_fruitsEaten < 7) _gameSpeed = random(130, 181); // Postupné zrychlování
                    else if (_fruitsEaten < 12) _gameSpeed = random(90, 111); // Rychlejší rychlost pro střední fázi
                    else _gameSpeed = random(40, 61); // Velmi rychlá rychlost pro pokročilou fázi, kdy had sní hodně ovoce
                } else {
                    _gameSpeed = _difficultySpeeds[_difficultyIndex]; // Nastavení rychlosti hry podle zvolené obtížnosti (pro lehkou, střední a těžkou obtížnost se rychlost nemění)
                }

                if (millis() - _lastMoveTime >= _gameSpeed) { // Kontrola, zda je čas pro další pohyb hada podle aktuální rychlosti hry
                _lastMoveTime = millis(); // Aktualizace času posledního pohybu

                for (int i = _snakeLength - 1; i > 0; i--) // Posun hada - každá část těla se posune na pozici části před ní
                    _snake[i] = _snake[i - 1]; // Posun hlavy hada - aktualizace pozice hlavy podle směru pohybu

                _snake[0].x += _dirX; // Aktualizace pozice hlavy hada v ose X
                _snake[0].y += _dirY; // Aktualizace pozice hlavy hada v ose Y

                // Kolize s hranami - pokud by had naboural do okraje mapy, dojde ke konci hry
                if (_snake[0].x < 1 || _snake[0].x >= 128/4 - 1 || // Kontrola v ose X
                    _snake[0].y < 1 || _snake[0].y >= 64/4 - 1) { // Kontrola v ose Y
                        gameOver(); // Pokud dojde ke kolizi, zavolá se funkce pro zpracování konce hry
                        break; // Přerušení dalšího zpracování pohybu hada po zjištění kolize s hranou mapy
                    }

                // Kolize s tělem - pokud by had měl nabourat sám do sebe, dojde ke konci hry
                for (int i = 1; i < _snakeLength; i++) { // Smyčka pro kontrolu kolize hlavy hada s jeho tělem
                    if (_snake[0].x == _snake[i].x && _snake[0].y == _snake[i].y){ // Kontrola, zda hlava hada koliduje s částí jeho těla
                        gameOver(); // Pokud dojde ke kolizi, zavolá se funkce pro zpracování konce hry
                        break; // Přerušení smyčky po zjištění kolize
                    }
                }

                // Sběr ovoce - pokud hlava hada najede na pole obsahující ovoce, dojde k zvýšení délky hada a haptické odezvě pomocí bzučáku
                if (_snake[0].x == _fruit.x && _snake[0].y == _fruit.y) { // Kontrola, zda hlava hada koliduje s pozicí ovoce
                    _snakeLength++; // Zvýšení délky hada o 1
                    _fruitsEaten++; // Zvýšení počtu snězeného ovoce o 1
                    spawnFruit(); // Generování nové pozice ovoce po jeho snězení
                    digitalWrite(controls.BUZZER_PIN, HIGH); // Aktivace bzučáku pro haptickou odezvu při snězení ovoce
                    delay(50); // Krátká prodleva pro zvuk bzučáku
                    digitalWrite(controls.BUZZER_PIN, LOW); // Deaktivace bzučáku po prodlevě
                }
                }

                // Vykreslení hry
                controls.display.clearDisplay(); // Vyčištění displeje před vykreslením aktuálního stavu hry
                controls.display.setTextSize(1); // Nastavení velikosti textu pro zobrazení hry
                controls.display.setTextColor(SH110X_WHITE); // Nastavení barvy textu pro zobrazení hry
                controls.display.drawRect(0, 0, 128, 64, SH110X_WHITE); // Vykerslení okraje mapy
                for (int i = 0; i < _snakeLength; i++) // Smyčka pro vykreslení jednotlivých částí hada
                controls.display.fillRect(_snake[i].x * 4, _snake[i].y * 4, 4, 4, SH110X_WHITE); // Vykreslení hada
                controls.display.fillRect(_fruit.x * 4, _fruit.y * 4, 4, 4, SH110X_WHITE); // Vykreslení ovoce
                controls.display.display(); // Aktualizace displeje pro zobrazení aktuálního stavu hry
                break;

        // Konec hry - pokud dojde ke konci hry z důvodu střetu s okrajem mapy anebo tělem hada, dojde k ukončení hry
            case GAME_OVER: // Vykreslení obrazovky pro konec hry
                controls.display.clearDisplay(); // Vyčištění displeje před vykreslením obrazovky pro konec hry
                controls.display.setTextSize(1); // Nastavení velikosti textu pro zobrazení konec hry
                controls.display.setTextColor(SH110X_WHITE); // Nastavení barvy textu pro zobrazení konec hry
                controls.display.setCursor(30, 0); // Nastavení pozice kurzoru pro nadpis konec hry
                controls.display.println("KONEC HRY"); // Vykreslení nadpisu pro konec hry
                controls.display.setCursor(0, 12); // Nastavení pozice kurzoru pro zobrazení počtu snězeného ovoce a zvolené obtížnosti
                controls.display.print("Ovoce: "); controls.display.println(_fruitsEaten); // Vykreslení počtu snězeného ovoce
                controls.display.setCursor(0, 22); // Nastavení pozice kurzoru pro zobrazení zvolené obtížnosti
                controls.display.print("Obtiznost: "); controls.display.println(_difficultyLevels[_difficultyIndex]); // Vykreslení zvolené obtížnosti
                controls.display.setCursor(0, 32); // Nastavení pozice kurzoru pro zobrazení instrukce pro zadání jména pro tabulku na webu
                controls.display.println("Web: zadejte jmeno pro tabulku"); // Vykreslení instrukce pro zadání jména pro tabulku na webu
                controls.display.setCursor(0, 52); // Nastavení pozice kurzoru pro zobrazení instrukce pro návrat do menu
                controls.display.println("Dotkni se pro navrat"); // Vykreslení instrukce pro návrat do menu
                controls.display.display(); // Aktualizace displeje pro zobrazení obrazovky pro konec hry
                break;
    }
}

void SnakeLib::handleButtonPress() { // Zpracování stisků tlačítek pro pohyb v menu, nastavení a změnu směru hada ve hře
    switch (_gameState) { // Zpracování stisků tlačítek podle aktuálního stavu hry
        case MENU: // Pohyb v menu - změna indexu vybrané položky menu
            if (controls.leftPressed){ // Pokud je stisknuto levé tlačítko, posuneme výběr v menu nahoru
                _menuIndex = (_menuIndex - 1 + _menuItemCount) % _menuItemCount;  // Posun indexu menu nahoru s ošetřením přetečení 
                controls.leftPressed = false; // Reset stavu tlačítka po zpracování stisku
            }

            if (controls.rightPressed){ // Pokud je stisknuto pravé tlačítko, posuneme výběr v menu dolů
                _menuIndex = (_menuIndex + 1) % _menuItemCount; /// Posun indexu menu dolů s ošetřením přetečení
                controls.rightPressed = false; // Reset stavu tlačítka po zpracování stisku
            }
            break;

        case SETTINGS: // Změna obtížnosti v nastavení - změna indexu zvolené obtížnosti
            if (controls.leftPressed){ // Pokud je stisknuto levé tlačítko, posuneme výběr obtížnosti nahoru
                _difficultyIndex = (_difficultyIndex + 1) % 4; // Posun indexu obtížnosti nahoru s ošetřením přetečení
                updateDifficultyLED(); // Aktualizace LED indikace pro zobrazení zvolené obtížnosti
                controls.leftPressed = false; // Reset stavu tlačítka po zpracování stisku
            }

            if (controls.rightPressed){ // Pokud je stisknuto pravé tlačítko, posuneme výběr obtížnosti dolů
                _difficultyIndex = (_difficultyIndex - 1 + 4) % 4; // Posun indexu obtížnosti dolů s ošetřením přetečení
                updateDifficultyLED(); // Aktualizace LED indikace pro zobrazení zvolené obtížnosti
                controls.rightPressed = false; // Reset stavu tlačítka po zpracování stisku
            }
            break;
        
        case GAME: // Změna směru pohybu hada ve hře - rotace doleva nebo doprava
            if (controls.leftPressed) { // Pokud je stisknuto levé tlačítko, otočíme směr pohybu hada doleva (proti směru hodinových ručiček)
                int oldX = _dirX; // Uložení původní hodnoty směru v ose X pro výpočet nové hodnoty směru v ose Y
                _dirX = _dirY; // Aktualizace směru v ose X podle směru v ose Y (otočení doleva)
                _dirY = -oldX; // Aktualizace směru v ose Y podle původní hodnoty směru v ose X (otočení doleva)
                controls.leftPressed = false; // Reset stavu tlačítka po zpracování stisku
            }
            if (controls.rightPressed) { // Pokud je stisknuto pravé tlačítko, otočíme směr pohybu hada doprava (ve směru hodinových ručiček)
                int oldX = _dirX; // Uložení původní hodnoty směru v ose X pro výpočet nové hodnoty směru v ose Y
                _dirX = -_dirY; // Aktualizace směru v ose X podle záporné hodnoty směru v ose Y (otočení doprava)
                _dirY = oldX; // Aktualizace směru v ose Y podle původní hodnoty směru v ose X (otočení doprava)
                controls.rightPressed = false; // Reset stavu tlačítka po zpracování stisku
            }
            break;
    }
    }

void SnakeLib::resetGame() {
    // Nastavení LED indikace pro zobrazení zvolené obtížnosti
    switch (_difficultyIndex) {
        case 0: controls.strip.setPixelColor(0, 0, 255, 0); break; // Lehká - zelená
        case 1: controls.strip.setPixelColor(0, 255, 165, 0); break; // Střední - oranžová
        case 2: controls.strip.setPixelColor(0, 255, 0, 0); break; // Těžká - červená
        case 3: controls.strip.setPixelColor(0, 128, 0, 255); break; // Chaos - fialová
    }
    controls.strip.show(); // Aktualizace stavu LED pro zobrazení zvolené obtížnosti

    // Reset hada
    _snake[0] = { // Nastavení počáteční pozice hada na náhodné místo na mapě s odstupem od okrajů
    random(3, 128/8 - 3),  // x: náhodná pozice hada na ose X, odstup od okrajů mapy
    random(3, 64/8 - 3)    // y: náhodná pozice hada na ose Y, odstup od okrajů mapy
    };
    _snakeLength = 2; // Počáteční délka hada
    _dirX = 1; // Směr pohybu hada v ose X (1 = doprava)
    _dirY = 0; // Směr pohybu hada v ose Y (0 = žádný pohyb v ose Y)
    _fruitsEaten = 0; // Reset počtu snězeného ovoce

    spawnFruit(); // Generování nové pozice ovoce na mapě

    _gameState = GAME; // Nastavení stavu hry na GAME pro spuštění samotné hry
    _lastMoveTime = millis(); // Aktualizace času posledního pohybu hada pro správné načasování pohybu podle zvolené obtížnosti
    }

void SnakeLib::gameOver() { // Zpracování konce hry - zobrazení obrazovky pro konec hry a haptická odezva pomocí bzučáku
    for (int i = 0; i < 3; i++) { // Opakování 3x pro vytvoření efektu blikání a zvuku bzučáku při konci hry
        digitalWrite(controls.BUZZER_PIN, HIGH); // Aktivace bzučáku pro haptickou odezvu při konci hry
        delay(100); // Krátká prodleva pro zvuk bzučáku
        digitalWrite(controls.BUZZER_PIN, LOW); // Deaktivace bzučáku po prodlevě
        delay(100); // Krátká prodleva mezi jednotlivými aktivacemi bzučáku pro vytvoření efektu blikání a zvuku při konci hry
    }
    _gameState = GAME_OVER; // Nastavení stavu hry na GAME_OVER pro zobrazení obrazovky pro konec hry
}

void SnakeLib::spawnFruit() { // Generování nové pozice ovoce na mapě - zajištění, že ovoce se neobjeví na pozici hada
    bool valid = false; // Proměnná pro kontrolu, zda je vygenerovaná pozice ovoce platná (nekoliduje s hadem)
    while (!valid) { // Smyčka pro generování nové pozice ovoce, dokud není nalezena platná pozice, která nekoliduje s hadem
        _fruit.x = random(3, 128/4 - 3); // x: náhodná pozice ovoce na ose X, odstup od okrajů mapy
        _fruit.y = random(3, 64/4 - 3); // y: náhodná pozice ovoce na ose Y, odstup od okrajů mapy
        valid = true; // Předpokládáme, že pozice ovoce je platná, dokud nenajdeme kolizi s hadem
        for (int i = 0; i < _snakeLength; i++) { // Smyčka pro kontrolu kolize vygenerované pozice ovoce s pozicí jednotlivých částí hada
            if (_snake[i].x == _fruit.x && _snake[i].y == _fruit.y) { // Kontrola, zda vygenerovaná pozice ovoce koliduje s pozicí části hada
                valid = false; // Pokud dojde ke kolizi, označíme pozici ovoce jako neplatnou a vygenerujeme novou pozici ovoce
                break; // Přerušení smyčky po zjištění kolize s hadem, aby se vygenerovala nová pozice ovoce
            }
        }
    }
}

void SnakeLib::handleTouchConfirm() { // Zpracování potvrzení dotyku pro výběr v menu, potvrzení nastavení a návrat do menu po konci hry
    switch (_gameState) { // Zpracování potvrzení dotyku podle aktuálního stavu hry
        case MENU: // Potvrzení výběru v menu - spuštění hry, přechod do nastavení nebo ukončení hry
            if (_menuIndex == 0) { // Start hry
                resetGame(); // Reset hry pro spuštění nové hry
            } else if (_menuIndex == 1) { // Nastavení obtížnosti
                _gameState = SETTINGS; // Přechod do stavu SETTINGS pro zobrazení menu nastavení a možnosti výběru obtížnosti
            } else if (_menuIndex == 2) { // Konec hry
                _exit = true; // Nastavení indikátoru pro ukončení hry a návrat do hlavního menu
            }
            break;

        case SETTINGS: // Potvrzení nastavení obtížnosti - návrat do menu a aktualizace rychlosti hry podle zvolené obtížnosti
            _gameSpeed = _difficultySpeeds[_difficultyIndex]; // Aktualizace rychlosti hry podle zvolené obtížnosti
            _gameState = MENU; // Přechod zpět do stavu MENU pro zobrazení hlavního menu
            break;

        case GAME_OVER:
            _gameState = MENU; // Přechod zpět do stavu MENU pro zobrazení hlavního menu po konci hry
            _menuIndex = 0; // Reset indexu menu na první položku
            _fruitsEaten = 0; // Reset počtu snězeného ovoce
    }
}

bool SnakeLib::shouldExit() { // Funkce pro kontrolu, zda je nastaven indikátor pro opuštění hry a návrat do hlavního menu
    return _exit; // Vrací hodnotu indikátoru pro opuštění hry
}

void SnakeLib::updateDifficultyLED() { // Aktualizace LED indikace pro zobrazení zvolené obtížnosti - každá obtížnost má svou barvu LED
    switch (_difficultyIndex) { // Nastavení barvy LED podle zvolené obtížnosti
        case 0: controls.strip.setPixelColor(0, 0, 255, 0); break; // Lehká - zelená
        case 1: controls.strip.setPixelColor(0, 255, 165, 0); break; // Střední - oranžová
        case 2: controls.strip.setPixelColor(0, 255, 0, 0); break; // Těžká - červená
        case 3: controls.strip.setPixelColor(0, 128, 0, 255); break; // Chaos - fialová
    }
    controls.strip.show(); // Aktualizace stavu LED pro zobrazení zvolené obtížnosti
}

void SnakeLib::web_SnakeLib() { // Registrace HTTP koncových bodů pro Snake hru

    web.server.on("/snake", [this]() { // Koncový bod pro načtení hlavní stránky hry Snake
        web.server.send_P(200, "text/html", snakePage); // Odeslání HTML stránky pro hru Snake
    });

    web.server.on("/snake/setDifficulty", [this]() { // Koncový bod pro nastavení obtížnosti hry Snake přes HTTP požadavky
        if (web.server.hasArg("level")) { // Kontrola, zda je v HTTP požadavku přítomen argument "level" pro nastavení obtížnosti
            int level = web.server.arg("level").toInt(); // Převedení hodnoty argumentu "level" na celé číslo pro nastavení obtížnosti
            setDifficulty(level); // Volání funkce pro nastavení obtížnosti hry Snake podle zadané hodnoty argumentu "level"
        }
        web.server.send(200, "text/plain", "Difficulty set"); // Odeslání odpovědi pro potvrzení nastavení obtížnosti
    });

    web.server.on("/snake/setName", [this]() { // Koncový bod pro nastavení jména hráče pro záznam
        if (web.server.hasArg("name")) { // Kontrola, zda je v HTTP požadavku přítomen argument "name" pro nastavení jména hráče
            String name = web.server.arg("name"); // Uložení zadaného jména hráče

            LeaderboardEntry entry; // Vytvoření záznamu pro leaderboard
            entry.name = name.length() ? name : "---"; // Nastavení jména hráče pro záznam, pokud není zadáno, použije se "---"
            entry.score = _fruitsEaten; // Nastavení skóre pro záznam podle počtu snězeného ovoce
            entry.difficulty = _difficultyLevels[_difficultyIndex]; // Nastavení obtížnosti pro záznam podle aktuální zvolené obtížnosti
            leaderboard.push_back(entry); // Přidání záznamu do tabulky pro pozdější zobrazení na webu
        }
        web.server.send(200, "text/plain", "OK"); // Odeslání odpovědi pro potvrzení nastavení jména hráče
    });

    web.server.on("/snake/leaderboard", [this]() { // Koncový bod pro načtení tabulky s výsledky hráčů pro zobrazení na webu
    std::sort(leaderboard.begin(), leaderboard.end(), // Seřazení záznamů v tabulce podle skóre a obtížnosti pro zobrazení na webu
      [](const LeaderboardEntry &a, const LeaderboardEntry &b) { // Lambda funkce pro porovnání záznamů v tabulce pro seřazení
          int multiplierA = (a.difficulty == "Lehka") ? 1 : // Nastavení násobitele 1x pro lehkou obtížnost
                            (a.difficulty == "Stredni") ? 2 : // Nastavení násobitele 2x pro střední obtížnost
                            (a.difficulty == "Tezka") ? 3 : // Nastavení násobitele 3x pro těžkou obtížnost
                            (a.difficulty == "Chaos") ? 5 : 0; // Nastavení násobitele 5x pro chaotickou obtížnost, jinak 0 pro neznámou obtížnost

          int multiplierB = (b.difficulty == "Lehka") ? 1 : // Nastavení násobitele 1x pro lehkou obtížnost
                            (b.difficulty == "Stredni") ? 2 : // Nastavení násobitele 2x pro střední obtížnost
                            (b.difficulty == "Tezka") ? 3 : // Nastavení násobitele 3x pro těžkou obtížnost
                            (b.difficulty == "Chaos") ? 5 : 0; // Nastavení násobitele 5x pro chaotickou obtížnost, jinak 0 pro neznámou obtížnost

          return (a.score * multiplierA) > (b.score * multiplierB); // Porovnání záznamů podle skóre vynásobeného násobitelem pro seřazení v sestupném pořadí

    });

    String json = "["; // Vytvoření JSON pole pro záznamy v tabulce pro odeslání na web
    for (size_t i = 0; i < leaderboard.size(); i++) { // Smyčka pro vytvoření JSON objektů pro jednotlivé záznamy v tabulce pro odeslání na web
        json += "{"; // Otevření JSON objektu pro záznam
        json += "\"name\":\"" + leaderboard[i].name + "\",";  // Přidání jména hráče do JSON objektu pro záznam
        json += "\"score\":" + String(leaderboard[i].score) + ","; // Přidání skóre hráče do JSON objektu pro záznam
        json += "\"difficulty\":\"" + leaderboard[i].difficulty + "\""; // Přidání obtížnosti hráče do JSON objektu pro záznam
        json += "}"; // Uzavření JSON objektu pro záznam
        if (i < leaderboard.size() - 1) json += ","; // Přidání čárky mezi záznamy v JSON poli, pokud nejde o poslední záznam
    }
    json += "]"; // Uzavření JSON pole pro záznamy v tabulce pro odeslání na web
    web.server.send(200, "application/json", json); // Odeslání JSON pole s výsledky hráčů pro zobrazení na webu
    });

    web.server.on("/snake/exit", [this]() { // Koncový bod pro opuštění Snake a návrat do hlavního menu přes HTTP požadavky
        _exit = true; // Nastavení indikátoru pro opuštění menu
        web.server.send(200,"text/plain","ok"); // Odeslání odpovědi pro potvrzení opuštění menu a návratu do hlavního menu
    });
}

void SnakeLib::setDifficulty(int level){ // Funkce pro nastavení obtížnosti hry Snake podle zadané hodnoty argumentu "level" z HTTP požadavku
    if(level < 0 || level > 3) return; // Ověření, že zadaná hodnota pro obtížnost je v platném rozsahu (0-3)

    _difficultyIndex = level; // Nastavení indexu zvolené obtížnosti podle zadané hodnoty argumentu "level"
    _gameSpeed = _difficultySpeeds[_difficultyIndex]; // Aktualizace rychlosti hry podle zvolené obtížnosti

    updateDifficultyLED(); // Aktualizace LED indikace pro zobrazení zvolené obtížnosti po změně obtížnosti
}