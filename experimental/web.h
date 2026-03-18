#ifndef WEB_H
#define WEB_H

#pragma once

#include <WebServer.h>

class Web {
    public:
        Web() = default;

        void start();       // Inicializace webového serveru
        void loop();        // Zpracování příchozích HTTP požadavků
        WebServer server{80}; // Vytvoření instance webového serveru na portu 80

};

extern Web web;

extern const char menuPage[];
extern const char rgbPage[];
extern const char snakePage[];
extern const char levelPage[];
extern const char ppgPage[];

#endif