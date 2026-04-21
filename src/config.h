#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- PIN HARDWARE ---
#define BTN_PIN     39
#define MATRIX_PIN  27
#define MATRIX_NUM  25

// --- TASTO TIMING ---
#define BTN_SHORT_MS   800
#define BTN_LONG_MS    2000
#define BTN_VLONG_MS   5000

// --- MODALITÀ ---
#define MODE_NONE       0
#define MODE_CUELIST    1
#define MODE_BLACKOUT   2
#define MODE_TALLY      3
#define MODE_SNAP       4
#define MODE_OVERRIDE   5

// --- STRUTTURA DATI PERSISTENTE ---
struct AtomConfig {
    char    ssid[32];
    char    pass[32];
    char    hostname[32];
    char    masterHost[64];
    char    masterName[32];
    uint8_t mode;
    uint8_t brightness;
    uint8_t use_dhcp;
    uint8_t ip[4];
    uint8_t gateway[4];
    uint8_t subnet[4];

    // Tally config
    char    tallyParams[8][16];
    uint8_t tallyCount;
    bool    tallyAutoplay;
    uint8_t tallyInterval;

    // Pattern matrice personalizzati
    uint8_t patterns[5][25][3];
    char    patternNames[5][16];

    // Cue list attiva
    uint8_t activeListIndex;
    float snapFade;   // fade per snap recall in secondi
};

extern AtomConfig settings;

void saveConfiguration();
void loadConfiguration();

#endif