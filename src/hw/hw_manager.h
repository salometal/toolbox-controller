#ifndef HW_MANAGER_H
#define HW_MANAGER_H

#include <Arduino.h>
#include <FastLED.h>
#include "../config.h"

extern CRGB matrix[MATRIX_NUM];

// Init e loop
void hw_init();
void hw_boot();
void hw_loop();

// Matrice
void matrix_fill(CRGB color);
void matrix_clear();
void matrix_show();
void matrix_set(uint8_t x, uint8_t y, CRGB color);
void matrix_brightness(uint8_t val);

// Animazioni
void matrix_blink(CRGB color, int times, int ms);
void matrix_identify();

// Tasto
typedef enum {
    BTN_NONE = 0,
    BTN_SHORT,
    BTN_LONG,
    BTN_VLONG
} BtnEvent;

BtnEvent btn_read();

#endif