#ifndef CUELIST_ENGINE_H
#define CUELIST_ENGINE_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "../config.h"

// --- STRUTTURA CUE ---
struct Cue {
    char    name[32];
    char    snapId[8];      // id snapshot Toolbox
    float   fade;           // secondi
    uint8_t goMode;         // 0=manuale, 1=autogo
    float   autogoDelay;    // secondi prima del go automatico
    uint8_t color[3];       // RGB colore UI
};

// --- STRUTTURA LISTA ---
struct CueList {
    char    name[32];
    Cue     cues[32];
    uint8_t count;
};

// Stato runtime
extern int      cl_activeList;      // indice lista attiva
extern int      cl_currentCue;      // indice cue corrente
extern bool     cl_running;         // lista in esecuzione
extern CueList  cl_lists[8];        // max 8 liste
extern uint8_t  cl_listCount;       // quante liste caricate

void cuelist_init();
void cuelist_loop();

void cuelist_go();          // avanza cue
void cuelist_back();        // torna indietro
void cuelist_load(int listIndex);
void cuelist_save();

#endif