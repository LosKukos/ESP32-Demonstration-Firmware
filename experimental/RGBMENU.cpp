#include <RGBMENU.h>
RGBMenu::RGBMenu(Controls& ctrl) : controls(ctrl), currentItem(0), adjustingValue(false) {
}

void RGBMenu::begin() { // Funkce volaná při aktivaci RGB menu

    currentItem = 0; // Nastavení výchozí položky menu
    _exit = false;   // Indikátor pro opuštění menu

    controls.strip.setPixelColor(0,0,0,0); // Vypnutí LED při vstupu do menu
    controls.strip.show(); // Aktualizace LED
}

void RGBMenu::loop() { // Hlavní smyčka pro RGB menu

    // Přepínání mezi slidery a navigací v menu. Uživatel buďto upravuje hodnotu slideru, nebo se pohybuje mezi položkami menu.
    if (controls.FingerTouchedFlag() && currentItem < 3) {
        adjustingValue = !adjustingValue;
    }

    // Navigace v menu, pokud není upravována hodnota slideru
    if (!adjustingValue) {
        if  (controls.leftPressed) {  // Tlačítko pro posun nahoru
            controls.leftPressed = false; // Reset stavu tlačítka
            currentItem = (currentItem - 1 + menuLength) % menuLength; // Posun nahoru s přelitím na konec menu
        }
        if  (controls.rightPressed) { // Tlačítko pro posun dolů
            controls.rightPressed = false; // Reset stavu tlačítka
            currentItem = (currentItem + 1) % menuLength; // Posun dolů s přelitím na začátek menu
        }
    }

    // Úprava hodnot sliderů, pokud je uživatel v režimu úpravy
    if (adjustingValue) {
        if (controls.leftPressed) {  // Tlačítko pro zvýšení hodnoty
            controls.leftPressed = false; // Reset stavu tlačítka
            sliderValues[currentItem] += 10; // Zvýšení hodnoty o 10
            if (sliderValues[currentItem] > 255) sliderValues[currentItem] = 255; // Omezení na max 255
        }
        if (controls.rightPressed) { // Tlačítko pro snížení hodnoty
            controls.rightPressed = false; // Reset stavu tlačítka
            sliderValues[currentItem] -= 10; // Snížení hodnoty o 10
            if (sliderValues[currentItem] < 0) sliderValues[currentItem] = 0; // Omezení na min 0
        }
    }

    // Pokud je vybrána položka "Restart", resetujeme všechny hodnoty sliderů
    if (currentItem == 3 && controls.fingerTouched()) { // Pokud dojde k potvrzení výběru "Restart" dotykem
        adjustingValue = false; // Zůstaneme v režimu navigace
        sliderValues[0] = sliderValues[1] = sliderValues[2] = 0; // Reset hodnot sliderů na 0
    }

    // Pokud je vybrána položka "Zpet", nastavíme indikátor pro opuštění modulu
    if (currentItem == 4 && controls.fingerTouched()) { // Pokud dojde k potvrzení výběru "Zpet" dotykem
        adjustingValue = false; // Zůstaneme v režimu navigace
        sliderValues[0] = sliderValues[1] = sliderValues[2] = 0; // Reset hodnot sliderů na 0
        _exit = true; // Nastavení indikátoru pro opuštění modulu zpět do hlavního menu
    }

    updateLED(); // Aktualizace LED podle aktuálních hodnot sliderů
    drawMenu(); // Vykreslení menu na OLED s aktuálními hodnotami a výběrem
}

// Vykreslení sliderů na displeji OLED
void RGBMenu::drawSlider(int x, int y, int w, int h, int value, const char* label, bool selected) {

    controls.display.drawRect(x, y, w, h, SH110X_WHITE); // Vykreslení obdélníku pro slider
    controls.display.fillRect(x, y, map(value, 0, 255, 0, w), h, SH110X_WHITE); // Vykreslení vyplněné části slideru podle hodnoty

    controls.display.setCursor(x + w + 4, y); // Nastavení kurzoru pro zobrazení hodnoty a popisku
    controls.display.print(label); // Zobrazení popisku (R, G, B)
    controls.display.print(" "); // Mezera mezi popiskem a hodnotou
    controls.display.print(value); // Zobrazení aktuální hodnoty slideru

    if (selected) { // Pokud je tento slider vybraný, vykreslíme indikátor výběru
        controls.display.setCursor(x - 6, y); // Nastavení kurzoru pro zobrazení indikátoru výběru
        controls.display.print(">"); // Zobrazení indikátoru výběru (šipka)
    }
}

// Vykreslení menu na displeji OLED
void RGBMenu::drawMenu() {
    controls.display.clearDisplay(); // Vyčištění displeje před vykreslením menu

    int yOffset = 2; // Posun od horního okraje pro zobrazení menu
    int spacing = 10; // Vzdálenost mezi jednotlivými položkami menu

    for (int i = 0; i < menuLength; i++) { // Pro každou položku menu
        if (i < 3) { // Pro první tři položky (R, G, B) vykreslíme slidery
            drawSlider(5, yOffset + i * spacing, 80, 8, sliderValues[i], menuItems[i], i == currentItem); // Vykreslení slideru pro R, G, B
        } else { // Pro položky "Restart" a "Zpet" vykreslíme pouze text
            controls.display.setCursor(5, yOffset + i * spacing + 10); // Posun pro zobrazení textu pod slidery
            if (i == currentItem) controls.display.print(">"); // Indikátor výběru pro položky "Restart" a "Zpet"
            controls.display.print(menuItems[i]); // Zobrazení textu pro položky "Restart" a "Zpet"
        }
    }

    // Hexadecimální zobrazení aktuální barvy RGB
    controls.display.setCursor(5, yOffset + 3 * spacing); // Posun pro zobrazení hex hodnoty pod položkami menu
    char hexColor[7]; // Buffer pro uložení hexadecimálního řetězce (6 znaků pro RGB + 1 pro null terminator)
    sprintf(hexColor, "%02X%02X%02X", sliderValues[0], sliderValues[1], sliderValues[2]); // Formátování RGB hodnot do hexadecimálního řetězce
    controls.display.print("HEX: #"); // Zobrazení textu "HEX: #" před hexadecimální hodnotou
    controls.display.print(hexColor); // Zobrazení hexadecimální hodnoty aktuální barvy RGB

    controls.display.display(); // Aktualizace displeje pro zobrazení vykresleného menu
}

void RGBMenu::updateLED() { // Aktualizace LED podle aktuálních hodnot sliderů
    controls.strip.setPixelColor(0, sliderValues[0], sliderValues[1], sliderValues[2]); // Nastavení barvy LED podle hodnot sliderů R, G, B
    controls.strip.show(); // Aktualizace LED pro zobrazení nové barvy
}

bool RGBMenu::shouldExit() { // Indikátor pro opuštění menu
    return _exit; // Vrací true, pokud uživatel zvolil opuštění menu, jinak false
}

// Webová stránka pro ovládání RGB menu, umožňuje aktualizaci hodnot sliderů a resetování nebo opuštění menu přes HTTP požadavky
void RGBMenu::web_RGBMenu() {

    web.server.on("/rgb", HTTP_GET, [this](AsyncWebServerRequest *request) { // Koncový bod pro zobrazení RGB menu
        request->send_P(200, "text/html", rgbPage); // Odeslání HTML stránky pro ovládání RGB menu
    });

    web.server.on("/rgb/update", HTTP_GET, [this](AsyncWebServerRequest *request) { // Koncový bod pro aktualizaci hodnot sliderů přes HTTP požadavky

        if (request->hasParam("r")) // Kontrola, zda je přítomen argument "r" pro hodnotu červené složky
            sliderValues[0] = constrain(request->getParam("r")->value().toInt(),0,255); // Aktualizace hodnoty červené složky s omezením na rozsah 0-255

        if (request->hasParam("g")) // Kontrola, zda je přítomen argument "g" pro hodnotu zelené složky
            sliderValues[1] = constrain(request->getParam("g")->value().toInt(),0,255); // Aktualizace hodnoty zelené složky s omezením na rozsah 0-255

        if (request->hasParam("b")) // Kontrola, zda je přítomen argument "b" pro hodnotu modré složky
            sliderValues[2] = constrain(request->getParam("b")->value().toInt(),0,255); // Aktualizace hodnoty modré složky s omezením na rozsah 0-255

        updateLED(); // Aktualizace LED pro zobrazení nové barvy podle aktualizovaných hodnot sliderů
        drawMenu();  // Aktualizace menu na OLED pro zobrazení nových hodnot sliderů a aktuální barvy RGB

        request->send(200,"text/plain","ok"); // Odeslání jednoduché odpovědi "ok" pro potvrzení úspěšné aktualizace hodnot sliderů
    });

    web.server.on("/rgb/reset", HTTP_GET, [this](AsyncWebServerRequest *request) { // Koncový bod pro resetování hodnot sliderů a LED přes HTTP požadavky

        sliderValues[0] = 0; // Reset hodnoty červené složky na 0
        sliderValues[1] = 0; // Reset hodnoty zelené složky na 0
        sliderValues[2] = 0; // Reset hodnoty modré složky na 0

        updateLED(); // Aktualizace LED pro zobrazení nové barvy (v tomto případě vypnutí LED, protože všechny složky jsou nastaveny na 0)
        drawMenu();  // Aktualizace menu na OLED pro zobrazení resetovaných hodnot sliderů a aktuální barvy RGB (vypnuté)

        request->send(200,"text/plain","ok"); // Odeslání jednoduché odpovědi "ok" pro potvrzení úspěšného resetování hodnot sliderů a LED
    });

    web.server.on("/rgb/exit", HTTP_GET, [this](AsyncWebServerRequest *request) { // Koncový bod pro opuštění RGB menu a návrat do hlavního menu přes HTTP požadavky

        _exit = true; // Nastavení indikátoru pro opuštění menu

        sliderValues[0] = 0; // Reset hodnoty červené složky na 0
        sliderValues[1] = 0; // Reset hodnoty zelené složky na 0
        sliderValues[2] = 0; // Reset hodnoty modré složky na 0

        updateLED(); // Aktualizace LED pro zobrazení nové barvy (v tomto případě vypnutí LED, protože všechny složky jsou nastaveny na 0)
        drawMenu();  // Aktualizace menu na OLED pro zobrazení resetovaných hodnot sliderů a aktuální barvy RGB (vypnuté)

        request->send(200,"text/plain","ok"); // Odeslání jednoduché odpovědi "ok" pro potvrzení úspěšného nastavení indikátoru pro opuštění menu a resetování hodnot sliderů a LED
    });

}
