#ifndef MATRIX_PATTERNS_H
#define MATRIX_PATTERNS_H

#include <Arduino.h>
#include "../hw/hw_manager.h"

// --- PRIORITÀ 1 ---
void mp_blackout();
void mp_running_off();

// --- PRIORITÀ 2 ---
void mp_crossfade_tick();   // chiamare ogni loop per animazione
void mp_artnet_searching(); // chiamare ogni loop per lampeggio

// --- CICLO ---
void mp_running_on();
void mp_scene(int8_t slotId);
void mp_universe(uint8_t universe);
void mp_mode(uint8_t mode);

// --- NUMERI E LETTERE ---
void mp_digit(uint8_t digit, CRGB color);
void mp_letter_D(CRGB color);
void mp_letter_A(CRGB color);
void mp_letter_K(CRGB color);
void mp_no_connection();

#endif