#ifndef NETWORK_ENGINE_H
#define NETWORK_ENGINE_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "../config.h"

void setupWebServer();
void networkTask(void *pvParameters);

// Polling Toolbox
void pollToolbox();
bool toolboxReachable();

// Stato Toolbox ricevuto
struct ToolboxStatus {
    bool    running;
    uint8_t mode;
    bool    blackout;
    bool    scene;
    bool    artnet;
    bool    crossfade;
    bool    keypad;      // ← aggiunto
    uint8_t universe;
    uint8_t refresh;
    int     heap;
    uint32_t uptime;
    char    hostname[32];
    int8_t  snapId;
    bool    valid;
};

extern ToolboxStatus toolboxStatus;
extern String wifiScanResults;

#endif