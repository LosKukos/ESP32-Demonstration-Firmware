#include <Wire.h>
#include <SnakeLib.h>
#include <rgbmenu.h>
#include <controls.h>
#include <settings.h>
#include <PPG.h>
#include <Level.h>
#include <web.h>

Controls controls;
auto& display = controls.display;
unsigned long lastStateChange = 0;
// Menu
const int menuLength = 5;
String menuItems[menuLength] = { "PPG", "Gyroskop", "Had", "Ovladani LED", "Nastaveni" };
int selected = 0;
unsigned long lastRainbowTime = 0;
int rainbowPos = 0;

int firstVisible = 0;             // index první viditelné položky
const int itemHeight = 12;        // výška jedné položky
const int topOffset = 20;         // kde začíná menu
const int visibleItems = 4;       // kolik položek se vejde na obrazovku

// Aktivní knihovny
SnakeLib snake(controls);
RGBMenu rgbMenu(controls);
Settings settings(controls);
PPG ppg(controls);
Level level(controls);

// Stav aplikace
enum AppState { MENU,
                APP_PPG,
                LEVEL_APP,
                SNAKE,
                RGB,
                SETTINGS };
AppState state = MENU;

// Vykreslení menu
void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  // Nadpis uprostřed nahoře
  display.setCursor((SCREEN_WIDTH - 8 * 11) / 2, 0);
  display.println("HLAVNI MENU");

  // vykreslení pouze viditelných položek
  for (int i = 0; i < visibleItems; i++) {
    int itemIndex = firstVisible + i;
    if (itemIndex >= menuLength) break;

    display.setCursor(10, topOffset + i * itemHeight);
    display.print(itemIndex == selected ? "> " : "  ");
    display.println(menuItems[itemIndex]);
  }

  display.display();
}

void setup() {
  Serial.begin(115200); // Spuštění sériové komunikace

  controls.begin(); // Inicializace periferií (display, WS2812, touch)

  delay(100);

  settings.Wifi_startup(); // Spuštění WiFi přístupového bodu ve výchozím nastavení

  delay(2000);

  ppg.start(); // Inicializace PPG modulu
  level.start(); // Inicializace vodováhy
  settings.start(); // Inicializace nastavení

  delay(100);

  // Registrace webů
  rgbMenu.web_RGBMenu();
  snake.web_SnakeLib(); 
  level.web_Level();
  Web_menu();
  

  web.server.onNotFound([]() {
      Serial.print("Stránka nebyla nalezena: ");
      Serial.println(web.server.uri());
      web.server.send(404, "text/plain", "Stránka nebyla nalezena");
    });

  web.start(); // Spuštění webového serveru

  // Příprava OLED
  display.setRotation(0);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.display();

  drawMenu();
}

void loop() {
  // MENU 
  if (state == MENU) {
    if (controls.leftPressed) {
      selected = (selected - 1 + menuLength) % menuLength;

      // pokud vyjedeš nahoru mimo viditelné okno
      if (selected < firstVisible) {
        firstVisible = selected;
      }
      // pokud se „wrap“ na poslední položku, posuň viewport dolů
      if (selected == menuLength - 1) {
        firstVisible = max(menuLength - visibleItems, 0);
      }

      controls.leftPressed = false;
      drawMenu();
    }

    if (controls.rightPressed) {
      selected = (selected + 1) % menuLength;

      // pokud vyjedeš dolů mimo viditelné okno
      if (selected >= firstVisible + visibleItems) {
        firstVisible = selected - visibleItems + 1;
      }
      // pokud se „wrap“ na první položku, posuň viewport nahoru
      if (selected == 0) {
        firstVisible = 0;
      }

      controls.rightPressed = false;
      drawMenu();
    }
  }

  // Stavové zpracování
  switch (state) {
    case MENU:
      {
        // Rainbow efekt
        unsigned long now = millis();

        if (now - lastRainbowTime >= 50) {
          lastRainbowTime = now;

          for (int i = 0; i < controls.strip.numPixels(); i++) {
            controls.strip.setPixelColor(i, Wheel((i + rainbowPos) & 255));
          }

          controls.strip.show();
          rainbowPos = (rainbowPos + 1) & 255;
        }

        // Touch výběr v menu
        if ((controls.FingerTouchedFlag()) && now - lastStateChange > 300) {
          switch (selected) {
            case 0:
              state = APP_PPG;
              ppg.begin();
              break;
            case 1:
              state = LEVEL_APP;
              level.begin();
              break;
            case 2:
              state = SNAKE;
              snake.begin();
              break;
            case 3:
              state = RGB;
              rgbMenu.begin();
              break;
            case 4:
              state = SETTINGS;
              settings.begin();
              break;
          }

          lastStateChange = now;  // zablokujeme touch hned po změně stavu
        }

        // Kreslení menu na OLED
        if (state == MENU) drawMenu();
      }
      break;
    case APP_PPG:
      ppg.loop();
      if (ppg.shouldExit()) {
        state = MENU;
        drawMenu();
        delay(50);  // malá prodleva (stabilizace touch)
      }
      break;

    case LEVEL_APP:
      level.loop();
      if (level.shouldExit()) {
        state = MENU;
        drawMenu();
        delay(50);  // malá prodleva (stabilizace touch)
      }
      break;

    case SNAKE:
      snake.loop();
      if (snake.shouldExit()) {
        state = MENU;
        drawMenu();
        delay(50);  // malá prodleva (stabilizace touch)
      }
      break;

    case RGB:
      rgbMenu.loop();
      if (rgbMenu.shouldExit()) {
        state = MENU;
        drawMenu();
        delay(50);
      }
      break;

    case SETTINGS:
      settings.loop();
      if (settings.shouldExit()){
        state = MENU;
        drawMenu();
        delay(50);
      }
      break;

  }
  delay(50);
}

uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return controls.strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return controls.strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return controls.strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void Web_menu() {
    web.server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", menuPage);
    });

    web.server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(204); // 204 = No Content
    });

    web.server.on("/rgb/activate", HTTP_GET, [](AsyncWebServerRequest *request) {
        rgbMenu.begin();  // inicializace RGB menu
        state = RGB;      // přepnutí stavu
        request->send(200, "text/plain", "ok");
    });

    web.server.on("/snake/activate", HTTP_GET, [](AsyncWebServerRequest *request) {
        snake.begin();  // inicializace snake
        state = SNAKE;  // přepnutí stavu
        request->send(200, "text/plain", "ok");
    });

    web.server.on("/level/activate", HTTP_GET, [](AsyncWebServerRequest *request) {
        level.begin();   // inicializace level
        state = LEVEL_APP; // přepnutí stavu
        request->send(200, "text/plain", "ok");
    });

    web.server.on("/ppg/activate", HTTP_GET, [](AsyncWebServerRequest *request) {
        level.begin();   // inicializace level
        state = APP_PPG; // přepnutí stavu
        request->send(200, "text/plain", "ok");
    });
}
