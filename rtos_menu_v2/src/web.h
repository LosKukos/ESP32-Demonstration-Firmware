#pragma once

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

class Web {
public:
    Web();
    void start();
    void Wifi_startup();

    AsyncWebServer& server();

private:
    AsyncWebServer _server;

    const char* SSID_default = "Bc";
};

extern Web web;