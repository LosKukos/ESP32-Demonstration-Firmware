#ifndef RGBMENU_H
#define RGBMENU_H

#include <controls.h>
#include <web.h>

class RGBMenu {
public:
    RGBMenu(Controls& ctrl); // Konstruktor pro inicializaci s referencí na modul Controls

    void begin();       // Inicializace RGB menu
    void loop();        // Hlavní smyčka pro RGB menu
    bool shouldExit();  // Indikuje, zda uživatel chce opustit RGB menu
    void web_RGBMenu(); // Webová stránka pro ovládání RGB

private:
    Controls& controls; // Reference na ovládací prvky (display, WS2812, touch) z modulu Controls

    static const int menuLength = 5; // Nastavení délky menu
    const char* menuItems[menuLength] = {"R", "G", "B", "Restart", "Zpet"}; // Položky menu

    int sliderValues[3] = {0,0,0};  // Výchozí hodnoty pro R, G, B
    int currentItem;                // Aktuálně vybraná položka
    bool adjustingValue;            // Indikátor, zda uživatel právě upravuje hodnotu slideru
    bool _exit;                     // Indikátor pro opuštění menu

    //  Vykreslovací funkce
    void drawMenu(); // Vykreslení menu na OLED
    void drawSlider(int x, int y, int w, int h, int value, const char* label, bool selected); // Vykreslení jednoho slideru
    void updateLED(); // Aktualizace LED podle aktuálních hodnot sliderů
    
};

#endif
