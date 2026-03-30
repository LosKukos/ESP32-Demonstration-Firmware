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
    WiFi.softAP(SSID_default);

    IPAddress ip = WiFi.softAPIP();

    Serial.print("AP IP: ");
    Serial.println(ip);
}