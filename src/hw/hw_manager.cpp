#include "hw_manager.h"
#include <WiFi.h>
#include "../core/mode_engine.h"

extern AtomConfig settings;

// --- TASTO ---
static uint32_t pressStart  = 0;
static bool     pressing    = false;
static bool     actionDone  = false;

void hw_init() {
    pinMode(BTN_PIN, INPUT);
    FastLED.addLeds<WS2812B, MATRIX_PIN, GRB>(matrix, MATRIX_NUM);
    FastLED.setBrightness(settings.brightness);
    matrix_clear();
    matrix_show();
    Serial.println("[HW] Matrice e tasto inizializzati.");
}

void hw_boot() {
    bool wifiOk = (WiFi.status() == WL_CONNECTED);
    CRGB color  = wifiOk ? CRGB(0, 200, 0) : CRGB(200, 0, 0);
    matrix_blink(color, 3, 300);
}

void hw_loop() {}

// --- MATRICE ---
void matrix_clear() {
    fill_solid(matrix, MATRIX_NUM, CRGB::Black);
}

void matrix_fill(CRGB color) {
    fill_solid(matrix, MATRIX_NUM, color);
}

void matrix_show() {
    FastLED.show();
}

void matrix_brightness(uint8_t val) {
    FastLED.setBrightness(val);
}

void matrix_set(uint8_t x, uint8_t y, CRGB color) {
    if (x >= 5 || y >= 5) return;
    matrix[y * 5 + x] = color;
}

void matrix_blink(CRGB color, int times, int ms) {
    for (int i = 0; i < times; i++) {
        matrix_fill(color);
        matrix_show();
        delay(ms);
        matrix_clear();
        matrix_show();
        delay(ms);
    }
}

void matrix_identify() {
    CRGB colors[] = {
        CRGB(255,0,0), CRGB(255,128,0), CRGB(255,255,0),
        CRGB(0,255,0), CRGB(0,0,255),   CRGB(128,0,255)
    };
    for (int r = 0; r < 3; r++) {
        for (auto& c : colors) {
            matrix_fill(c);
            matrix_show();
            delay(120);
        }
    }
    matrix_blink(CRGB::White, 5, 200);
    matrix_clear();
    matrix_show();
}

// --- TASTO ---
BtnEvent btn_read() {
    bool pressed = (digitalRead(BTN_PIN) == LOW);

    if (pressed && !pressing) {
        delay(20); // debounce
        if (digitalRead(BTN_PIN) != LOW) return BTN_NONE;
        pressing   = true;
        pressStart = millis();
        actionDone = false;
    }

    if (pressed && pressing && !actionDone) {
        uint32_t dur = millis() - pressStart;

        if (dur >= BTN_VLONG_MS) {
            if (appState != APP_MODE_SELECT) {
                appState    = APP_MODE_SELECT;
                pendingMode = settings.mode > 0 ? settings.mode : 1;
                matrix_show_menu(pendingMode);
            }
        } else if (dur >= BTN_LONG_MS) {
            static uint32_t lastBlink = 0;
            static bool blinkOn = false;
            if (millis() - lastBlink > 300) {
                blinkOn = !blinkOn;
                lastBlink = millis();
                if (blinkOn) matrix_fill(CRGB(50, 50, 50));
                else         matrix_show_mode(settings.mode);
                matrix_show();
            }
        }
        // short — nessun feedback
    }

    if (!pressed && pressing) {
        pressing = false;
        if (actionDone) return BTN_NONE;

        uint32_t dur = millis() - pressStart;
        Serial.printf("[BTN] Durata: %d ms\n", dur);
        actionDone = true;

        if (dur >= BTN_VLONG_MS) return BTN_VLONG;
        if (dur >= BTN_LONG_MS)  return BTN_LONG;
        if (dur >= 50)           return BTN_SHORT;
    }

    return BTN_NONE;
}