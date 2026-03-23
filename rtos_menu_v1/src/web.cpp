#include "web.h"

Web web; // Vytvoření instance třídy Web

Web::Web() : _server(80) {}

AsyncWebServer& Web::server() {
    return _server;
}

void Web::start() {
    _server.begin(); // spuštění serveru
}

void Web::Wifi_startup() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(SSID_default, PASS_default);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    IPAddress ip = WiFi.softAPIP();

    Serial.print("AP IP: ");
    Serial.println(ip);
}

// Webové stránky

    const char levelPage[] PROGMEM = R"rawliteral(
        <!DOCTYPE html>
        <html lang="cs">
        <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1.0">
            <title>Level Page - ESP32 Demo</title>
            <style>
            body {
                font-family: Arial, sans-serif;
                background-color: #222;
                color: #fff;
                margin: 0;
                padding: 0;
            }
            header {
                background-color: #444;
                padding: 15px;
                text-align: center;
                font-size: 1.5em;
            }
            nav {
                display: flex;
                flex-direction: column;
                padding: 0;
                margin: 0;
                list-style: none;
                align-items: center;
                margin-top: 50px;
            }
            nav a {
                display: block;
                padding: 15px 30px;
                margin: 5px 0;
                background-color: #555;
                color: #fff;
                text-decoration: none;
                border-radius: 8px;
                text-align: center;
                font-size: 1.2em;
                transition: background 0.3s;
            }
            nav a:hover {
                background-color: #ff6600;
            }
            footer {
                text-align: center;
                padding: 10px;
                font-size: 0.9em;
                color: #aaa;
                position: fixed;
                width: 100%;
                bottom: 0;
            }
            </style>
        </head>
        <body>
            <header>Level Page</header>
            <nav>
                <a id="backBtn" href="/">Zpět</a>
            </nav>
            <footer>Demonstrační firmware pro ESP32</footer>
        <script>
            document.getElementById("backBtn").addEventListener("click",function(){
            fetch("/level/exit");
            window.location.href="/";
            });
        </script>
        </body>
        </html>
    )rawliteral";