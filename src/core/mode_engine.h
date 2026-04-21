#ifndef MODE_ENGINE_H
#define MODE_ENGINE_H

#include <Arduino.h>
#include "../config.h"
#include "../hw/hw_manager.h"

// Stato macchina modale
typedef enum {
    APP_NORMAL = 0,      // funzionamento normale
    APP_MODE_SELECT,     // menu selezione modalità (very long press)
} AppState;

extern AppState appState;
extern uint8_t  pendingMode;  // modalità evidenziata nel menu

void mode_init();
void mode_loop();

// Azioni per modalità
void mode_do_short();   // click breve
void mode_do_long();    // long press
void mode_do_vlong();   // very long press → entra in menu

// Menu selezione modalità
void menu_next();       // scorre modalità
void menu_confirm();    // conferma modalità selezionata

// Pattern LED per ogni modalità
void matrix_show_mode(uint8_t mode);
void matrix_show_menu(uint8_t pendingMode);

#endif