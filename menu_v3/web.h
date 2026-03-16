#ifndef WEB_H
#define WEB_H

#pragma once

#include <ESPAsyncWebServer.h>

class Web {
    public:
        Web() = default;

        void start();       // Inicializace webového serveru
        AsyncWebServer server{80};

};

extern Web web;

extern const char menuPage[];
extern const char rgbPage[];
extern const char snakePage[];

#endif
