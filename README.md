# Bakalářská práce na téma Vývoj demonstračního firmware pro výukovou desku s platformou ESP32

Aktuální obsah:
1. Controls - pomocná knihovna pro zprostředkování základních funkcí v projektu - inicializace, ovládací prvky, I/O
2. Level - modul pro zpracování dat z gyroskopu a akcelerometru, vykreslení kuličky na OLED, která se naklání společně s senzorem
3. PPG - Snímání pulzní vlny pomocí IR/RED LEDky, vykreslení vlny na OLED, výpočet BPM, odesílání na připojené zařízení skrze sériovou linku a sériovou linku bluetooth
4. RGBMENU - modul založený na možnosti výběru složek barvy - R, G, B a namixování, zobrazení kódu na OLED a změna barvy WS2812B LED diody
5. Settings - pomocný modul, zabývá se nastavením konektivity jako je WiFi, Bluetooth, případně MQTT. Umožňuje uživateli výběr sítě a název zařízení
6. Snake - modul zpracovávající hru Snake, obsahuje 3 obtížnosti - lehká, střední, těžká, při konzumaci ovoce zapojena haptická odezva.

Roadmap
Aktuální moduly
  1. Controls.cpp - Modul dokončen, Web není k dispozici z důvodu vlastností modulu.
  2. Level.cpp - Modul dokončen, Web dokončen.
  3. Menu.ino -Modul dokončen, Web dokončen.
  4. PPG.cpp - Modul dokončen, Web dokončen.
  5. RGBMENU.cpp - Modul dokončen, Web dokončen.
  6. Settings.cpp - WIP, Web nebude k dispozici, místo toho bluetooth rozhraní.
  7. SnakeLib.cpp - Modul dokončen, Web dokončen.
