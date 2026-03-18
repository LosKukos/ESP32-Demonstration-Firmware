#include <AsyncTCP.h>

#include <ESPAsyncWebServer.h>

#ifndef WEB_H
#define WEB_H

#pragma once

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
extern const char levelPage[];
extern const char ppgPage[];

#endif
