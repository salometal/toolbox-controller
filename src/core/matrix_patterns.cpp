#include "matrix_patterns.h"

// ==========================================================
// FONT — numeri 0-9
// ==========================================================
static const uint8_t FONT_DIGITS[10][5] = {
    {0b01110, 0b10001, 0b10001, 0b10001, 0b01110}, // 0
    {0b00100, 0b01100, 0b00100, 0b00100, 0b01110}, // 1
    {0b01110, 0b00001, 0b00110, 0b01000, 0b01110}, // 2 -- fix
    {0b01110, 0b00001, 0b00110, 0b00001, 0b01110}, // 3
    {0b00010, 0b00110, 0b01010, 0b11111, 0b00010}, // 4 -- fix
    {0b11110, 0b10000, 0b01110, 0b00001, 0b11110}, // 5 -- fix
    {0b01110, 0b10000, 0b11110, 0b10001, 0b01110}, // 6
    {0b11111, 0b00001, 0b00010, 0b00100, 0b00100}, // 7 -- fix
    {0b01110, 0b10001, 0b01110, 0b10001, 0b01110}, // 8
    {0b01110, 0b10001, 0b01111, 0b00001, 0b01110}, // 9
};

// ==========================================================
// FONT — lettere
// ==========================================================
static const uint8_t FONT_D[5] = {0b11110, 0b10001, 0b10001, 0b10001, 0b11110};
static const uint8_t FONT_A[5] = {0b00100, 0b01010, 0b11111, 0b10001, 0b10001};
static const uint8_t FONT_K[5] = {0b10001, 0b10010, 0b11100, 0b10010, 0b10001};

// ==========================================================
// UTILITY — disegna pattern 5x5 da array di byte
// ==========================================================
static void draw_pattern(const uint8_t pattern[5], CRGB color) {
    matrix_clear();
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            if (pattern[y] & (1 << (4 - x))) {
                matrix_set(x, y, color);
            }
        }
    }
    matrix_show();
}

// ==========================================================
// PRIORITÀ 1
// ==========================================================

// Cerchio con barra diagonale — rosso
void mp_blackout() {
    const uint8_t pattern[5] = {
        0b01110,
        0b10001,  // modificato per barra
        0b10101,
        0b11001,
        0b01110
    };
    // Disegno manuale per la barra diagonale
    matrix_clear();
    // Cerchio
    matrix_set(1,0,CRGB(200,0,0)); matrix_set(2,0,CRGB(200,0,0)); matrix_set(3,0,CRGB(200,0,0));
    matrix_set(0,1,CRGB(200,0,0)); matrix_set(4,1,CRGB(200,0,0));
    matrix_set(0,2,CRGB(200,0,0)); matrix_set(4,2,CRGB(200,0,0));
    matrix_set(0,3,CRGB(200,0,0)); matrix_set(4,3,CRGB(200,0,0));
    matrix_set(1,4,CRGB(200,0,0)); matrix_set(2,4,CRGB(200,0,0)); matrix_set(3,4,CRGB(200,0,0));
    // Barra diagonale
    matrix_set(1,1,CRGB(200,0,0));
    matrix_set(2,2,CRGB(200,0,0));
    matrix_set(3,3,CRGB(200,0,0));
    matrix_show();
}

// Quadrato stop — rosso
void mp_running_off() {
    const uint8_t pattern[5] = {
        0b00000,
        0b01110,
        0b01110,
        0b01110,
        0b00000
    };
    draw_pattern(pattern, CRGB(200, 0, 0));
}

// ==========================================================
// PRIORITÀ 2
// ==========================================================

// Onda diagonale che riempie e svuota — azzurro
void mp_crossfade_tick() {
    static int8_t step = 8;
    static bool filling = true;
    static uint32_t lastTick = 0;

    if (millis() - lastTick < 80) return;
    lastTick = millis();

    matrix_clear();
    CRGB c = CRGB(0, 180, 255);

    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            if ((x + y) >= step) matrix_set(x, y, c);
        }
    }
    matrix_show();

    if (filling) {
        step--;
        if (step <= 0) filling = false;
    } else {
        step++;
        if (step >= 8) filling = true;
    }
}

// Lettera A gialla lampeggiante — no segnale ArtNet
void mp_artnet_searching() {
    static bool on = true;
    static uint32_t lastBlink = 0;

    if (millis() - lastBlink > 400) {
        on = !on;
        lastBlink = millis();
    }

    if (on) {
        draw_pattern(FONT_A, CRGB(200, 200, 0));
    } else {
        matrix_clear();
        matrix_show();
    }
}

// ==========================================================
// CICLO
// ==========================================================

// Triangolo play — verde
void mp_running_on() {
    const uint8_t pattern[5] = {
        0b01000,
        0b01100,
        0b01110,
        0b01100,
        0b01000
    };
    draw_pattern(pattern, CRGB(0, 200, 0));
}

// Numero slot snap — ciano
void mp_scene(int8_t slotId) {
    if (slotId < 0 || slotId > 9) {
        // slot sconosciuto — cornice
        const uint8_t pattern[5] = {
            0b11111,
            0b10001,
            0b10001,
            0b10001,
            0b11111
        };
        draw_pattern(pattern, CRGB(0, 200, 200));
        return;
    }
    mp_digit((uint8_t)slotId, CRGB(0, 200, 200));
}

// Numero universo — blu
void mp_universe(uint8_t universe) {
    mp_digit(universe % 10, CRGB(0, 0, 200));
}

// Lettera modalità — grigio
void mp_mode(uint8_t mode) {
    switch (mode) {
        case 0: draw_pattern(FONT_D, CRGB(150, 150, 150)); break; // DMX
        case 1: draw_pattern(FONT_A, CRGB(150, 150, 150)); break; // ArtNet
        case 2: draw_pattern(FONT_K, CRGB(150, 150, 150)); break; // Keypad
        default: matrix_clear(); matrix_show(); break;
    }
}

// ==========================================================
// UTILITY PUBBLICA
// ==========================================================
void mp_digit(uint8_t digit, CRGB color) {
    if (digit > 9) digit = 9;
    draw_pattern(FONT_DIGITS[digit], color);
}

void mp_letter_D(CRGB color) { draw_pattern(FONT_D, color); }
void mp_letter_A(CRGB color) { draw_pattern(FONT_A, color); }
void mp_letter_K(CRGB color) { draw_pattern(FONT_K, color); }
void mp_no_connection() {
    static bool on = true;
    static uint32_t last = 0;
    if (millis() - last > 500) { on = !on; last = millis(); }
    if (on) draw_pattern(FONT_A, CRGB(200, 0, 0));
    else { matrix_clear(); matrix_show(); }
}