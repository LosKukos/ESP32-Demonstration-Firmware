#include <Wire.h>
#include <SnakeLib.h>
#include <rgbmenu.h>
#include <controls.h>
#include <settings.h>

Controls controls;
auto& display = controls.display;
unsigned long lastStateChange = 0;
// --- Menu ---
const int menuLength = 5;
String menuItems[menuLength] = { "PPG", "EKG", "Snake", "RGB", "Nastaveni" };
int selected = 0;
unsigned long lastRainbowTime = 0;
int rainbowPos = 0;

// --- Aktivní knihovny ---
SnakeLib snake(controls);
RGBMenu rgbMenu(controls);
Settings settings(controls);
// bool ppgActive = false;
// bool ekgActive = false;
// bool vodovahaActive = false;

// --- Stav aplikace ---
enum AppState { MENU,
                PPG,
                EKG,
                SNAKE,
                RGB,
                SETTINGS };
AppState state = MENU;

// --- Vykreslení menu ---
void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  // Nadpis uprostřed nahoře
  display.setCursor((SCREEN_WIDTH - 8 * 11) / 2, 0);
  display.println("TESTOVACI FW");

  for (int i = 0; i < menuLength; i++) {
    display.setCursor(10, 20 + i * 12);
    display.print(i == selected ? "> " : "  ");
    display.println(menuItems[i]);
  }

  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(100);

  controls.begin();

  settings.begin();

  display.setRotation(0);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.display();

  drawMenu();
}

void loop() {
  // --- MENU ---
  if (state == MENU) {
      if (controls.leftPressed || controls.EncoderLeft) {
          selected = (selected - 1 + menuLength) % menuLength;
          controls.leftPressed = false;
          controls.EncoderLeft = false;
          drawMenu();
      }
      if (controls.rightPressed || controls.EncoderRight) {
          selected = (selected + 1) % menuLength;
          controls.rightPressed = false;
          controls.EncoderRight = false;
          drawMenu();
      }
  }

  // --- Stavové zpracování ---
  switch (state) {
    case MENU:
      {
        // --- Rainbow efekt ---
        unsigned long now = millis();

        if (now - lastRainbowTime >= 50) {
          lastRainbowTime = now;

          for (int i = 0; i < controls.strip.numPixels(); i++) {
            controls.strip.setPixelColor(i, Wheel((i + rainbowPos) & 255));
          }

          controls.strip.show();
          rainbowPos = (rainbowPos + 1) & 255;
        }

        // --- Touch / enkodér výběr v menu ---
        if ((controls.FingerTouchedFlag() || controls.EncoderButton) && now - lastStateChange > 300) {
          controls.EncoderButton = false;
          switch (selected) {
            case 0:
              state = PPG;
              break;
            case 1:
              state = EKG;
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
              settings.drawMenu();
              break;
          }

          lastStateChange = now;  // zablokujeme touch hned po změně stavu
        }

        // --- Kreslení menu na OLED ---
        drawMenu();
      }
      break;
    case PPG:
      // sem kód PPG
      if (controls.FingerTouchedFlag() && millis() - lastStateChange > 300) {
        state = MENU;
        lastStateChange = millis();
        drawMenu();
      }
      break;

    case EKG:
      // sem kód EKG
      if (controls.FingerTouchedFlag() && millis() - lastStateChange > 300) {
        state = MENU;
        lastStateChange = millis();
        drawMenu();
      }
      break;

    case SNAKE:
      snake.loop();
      if (snake.shouldExit()) {
        snake.exit();
        state = MENU;
        drawMenu();
        delay(50);  // malá prodleva (stabilizace touch)
      }
      break;
    case RGB:
      rgbMenu.loop();
      if (rgbMenu.shouldExit()) {
        rgbMenu.exit();  // vypnout WS2812
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
