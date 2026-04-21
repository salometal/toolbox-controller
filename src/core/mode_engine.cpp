#include "mode_engine.h"
#include "../net/network_engine.h"
#include "cuelist_engine.h"
#include "matrix_patterns.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

extern AtomConfig settings;
extern ToolboxStatus toolboxStatus;
// Forward declarations
void load_snap_list();

AppState appState  = APP_NORMAL;
uint8_t pendingMode = 0;

// Polling Toolbox — chiamato periodicamente dal loop
static uint32_t lastPoll     = 0;
static uint32_t lastTallyIdx = 0;
static uint8_t  tallyIdx     = 0;
// Snap Recall state
static int8_t  snapCurrentIdx = 0;
static uint8_t snapIds[50];
static char    snapNames[50][32];
static uint8_t snapCount = 0;
static bool    snapLoaded = false;



// ==========================================================
// PATTERN LED MODALITÀ
// ==========================================================

// Colori base per ogni modalità
static CRGB modeColor(uint8_t mode) {
    switch (mode) {
        case MODE_NONE:      return CRGB(50, 50, 50);    // grigio
        case MODE_CUELIST:   return CRGB(0, 0, 200);     // blu
        case MODE_BLACKOUT:  return CRGB(200, 0, 0);     // rosso
        case MODE_TALLY:     return CRGB(0, 200, 0);     // verde
        case MODE_SNAP:      return CRGB(200, 100, 0);   // arancio
        case MODE_OVERRIDE:  return CRGB(150, 0, 200);   // viola
        default:             return CRGB(50, 50, 50);
    }
}

// Mostra modalità attiva sulla matrice
void matrix_show_mode(uint8_t mode) {
    matrix_clear();
    CRGB c = modeColor(mode);

    switch (mode) {
        case MODE_NONE:
            // Croce centrale
            matrix_set(2, 1, c); matrix_set(2, 3, c);
            matrix_set(1, 2, c); matrix_set(3, 2, c);
            matrix_set(2, 2, c);
            break;

        case MODE_CUELIST:
            // Tre righe orizzontali (lista)
            for (int x = 1; x < 4; x++) {
                matrix_set(x, 1, c);
                matrix_set(x, 2, c);
                matrix_set(x, 3, c);
            }
            break;

        case MODE_BLACKOUT:
            // Tutto spento tranne bordo rosso
            for (int x = 0; x < 5; x++) {
                matrix_set(x, 0, c); matrix_set(x, 4, c);
            }
            for (int y = 1; y < 4; y++) {
                matrix_set(0, y, c); matrix_set(4, y, c);
            }
            break;

        case MODE_TALLY:
            // Occhio — cerchio centrale
            matrix_set(2, 1, c); matrix_set(1, 2, c);
            matrix_set(2, 2, c); matrix_set(3, 2, c);
            matrix_set(2, 3, c);
            break;

        case MODE_SNAP:
            // Quattro angoli + centro
            matrix_set(0, 0, c); matrix_set(4, 0, c);
            matrix_set(0, 4, c); matrix_set(4, 4, c);
            matrix_set(2, 2, c);
            break;

        case MODE_OVERRIDE:
            // Diagonale
            for (int i = 0; i < 5; i++) matrix_set(i, i, c);
            break;
    }
    matrix_show();
}

// Menu selezione — mostra modalità pendente con bordo bianco
void matrix_show_menu(uint8_t pending) {
    matrix_clear();
    CRGB c = modeColor(pending);

    // Pattern lettere 5x5 — riga per riga, bit 4=sinistra bit 0=destra
    // C=CueList, B=Blackout, T=Tally, S=Snap, G=Grab
    const uint8_t letters[6][5] = {
        {0b00000, 0b00000, 0b00000, 0b00000, 0b00000}, // 0 MODE_NONE — tutto spento
        {0b01111, 0b10000, 0b10000, 0b10000, 0b01111}, // 1 C — CueList
        {0b11100, 0b10010, 0b11100, 0b10010, 0b11100}, // 2 B — Blackout
        {0b11111, 0b00100, 0b00100, 0b00100, 0b00100}, // 3 T — Tally
        {0b01111, 0b10000, 0b01110, 0b00001, 0b11110}, // 4 S — Snap
        {0b01111, 0b10000, 0b10111, 0b10001, 0b01110}, // 5 G — Grab
    };

    uint8_t idx = (pending <= 5) ? pending : 0;

    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            // bit 4 = colonna 0 (sinistra), bit 0 = colonna 4 (destra)
            if (letters[idx][y] & (1 << (4 - x))) {
                matrix_set(x, y, c);
            }
        }
    }

    matrix_show();
}

// ==========================================================
// AZIONI MODALITÀ
// ==========================================================

// --- CUE LIST ---
void action_cuelist_short() {
    // GO — avanza alla prossima cue
    cuelist_go();
}

void action_cuelist_long() {
    // Torna alla cue precedente
    cuelist_back();
}

// --- BLACKOUT ---
void action_blackout_short() {
    if (!toolboxReachable()) return;

    HTTPClient http;
    if (toolboxStatus.blackout) {
        http.begin("http://" + String(settings.masterHost) + "/api/snap/release");
    } else {
        http.begin("http://" + String(settings.masterHost) + "/api/blackout");
    }
    http.setTimeout(2000);
    http.GET();
    http.end();

    // Feedback visivo
    CRGB c = toolboxStatus.blackout ? CRGB(0,200,0) : CRGB(200,0,0);
    matrix_blink(c, 2, 150);
    matrix_show_mode(settings.mode);
}

// --- TALLY --- (solo display, nessuna azione su short)
void action_tally_short() {
    // Click manuale scorre il parametro visualizzato
    tallyIdx = (tallyIdx + 1) % max((uint8_t)1, settings.tallyCount);
    lastTallyIdx = millis();
}

// --- SNAP RECALL ---
void action_snap_short() {
    if (!snapLoaded) {
        load_snap_list();
        return;
    }
    if (snapCount == 0) return;
    snapCurrentIdx = (snapCurrentIdx + 1) % snapCount;
    Serial.printf("[SNAP] Snap %d: %s\n", snapIds[snapCurrentIdx], snapNames[snapCurrentIdx]);
}

void action_snap_long() {
    if (!toolboxReachable()) return;
    if (snapCount == 0) { load_snap_list(); return; }

    HTTPClient http;
    String url = "http://" + String(settings.masterHost) +
                "/api/snap/recall?id=" + String(snapIds[snapCurrentIdx]) +
                "&fade=" + String(settings.snapFade, 1);
    http.begin(url);
    http.setTimeout(2000);
    int code = http.GET();
    http.end();

    Serial.printf("[SNAP] Recall snap %d code %d\n", snapIds[snapCurrentIdx], code);

    if (code == 200) {
        matrix_blink(CRGB(200, 100, 0), 2, 150);
    } else {
        matrix_blink(CRGB(200, 0, 0), 2, 150);
    }
}

// --- SCENE OVERRIDE ---
void action_override_long() {
        Serial.println("[GRAB] action_override_long chiamata");
    if (!toolboxReachable()) {
        Serial.println("[GRAB] Toolbox non raggiungibile");
        matrix_blink(CRGB(200, 0, 0), 3, 200);
        return;
    }
    
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;

    // 1. Recupera lista snapshot per trovare slot libero
    String url = "http://" + String(settings.masterHost) + "/api/snapshots";
    http.begin(url);
    http.setTimeout(2000);
    int code = http.GET();

    if (code != 200) {
        http.end();
        matrix_blink(CRGB(200, 0, 0), 3, 200);
        matrix_show_mode(settings.mode);
        return;
    }

    String body = http.getString();
    http.end();

    // 2. Trova primo slot libero e numero incrementale
    StaticJsonDocument<512> doc;
    deserializeJson(doc, body);
    JsonArray snaps = doc.as<JsonArray>();

    // Costruisci set degli slot occupati
    bool occupied[10] = {false};
    int grabCount = 0;
    for (JsonObject s : snaps) {
        int id = s["id"] | -1;
        if (id >= 0 && id < 10) occupied[id] = true;
        // Conta quanti GRAB esistono già per nome incrementale
        String name = s["name"] | "";
        if (name.startsWith("GRAB_")) grabCount++;
    }

    // Trova primo slot libero
    int freeSlot = -1;
    for (int i = 0; i < 10; i++) {
        if (!occupied[i]) { freeSlot = i; break; }
    }

    // 3. Nessuno slot libero — lampeggio rosso
    if (freeSlot < 0) {
        Serial.println("[GRAB] Nessuno slot libero!");
        matrix_blink(CRGB(200, 0, 0), 3, 200);
        matrix_show_mode(settings.mode);
        return;
    }

    // 4. Salva snapshot
    grabCount++;
    String grabName = "GRAB_" + String(grabCount < 10 ? "0" : "") + String(grabCount);
    String saveUrl = "http://" + String(settings.masterHost) +
                     "/save_snap?id=" + String(freeSlot) +
                     "&name=" + grabName;

    http.begin(saveUrl);
    http.setTimeout(2000);
    code = http.GET();
    http.end();

    Serial.printf("[GRAB] Slot %d nome %s code %d\n", freeSlot, grabName.c_str(), code);

    if (code == 200) {
        // Conferma — 3 lampi viola
        matrix_blink(CRGB(150, 0, 200), 3, 200);
    } else {
        // Errore salvataggio — rosso
        matrix_blink(CRGB(200, 0, 0), 3, 200);
    }
    matrix_show_mode(settings.mode);
}
// ==========================================================
// MENU SELEZIONE MODALITÀ
// ==========================================================
void menu_next() {
    pendingMode = (pendingMode % 5) + 1; // cicla 1-5
    matrix_show_menu(pendingMode);
}

void menu_confirm() {
    settings.mode = pendingMode;
    appState = APP_NORMAL;
    saveConfiguration();
    matrix_blink(modeColor(settings.mode), 2, 200);
    matrix_show_mode(settings.mode);

    // Carica snap list se si entra in modalità SNAP
    if (settings.mode == MODE_SNAP) {
        snapLoaded = false;
        load_snap_list();
    }
    Serial.printf("[MODE] Modalità confermata: %d\n", settings.mode);
}

// ==========================================================
// DISPATCHER TASTO
// ==========================================================
void mode_do_short() {
    if (appState == APP_MODE_SELECT) {
        menu_next();
        return;
    }
    switch (settings.mode) {
        case MODE_CUELIST:  action_cuelist_short();  break;
        case MODE_BLACKOUT: action_blackout_short();  break;
        case MODE_TALLY:    action_tally_short();     break;
        case MODE_SNAP:     action_snap_short();      break;
        case MODE_OVERRIDE: break; // nessuna azione su short
        default: break;
    }
}

void mode_do_long() {
    Serial.printf("[MODE] long press — mode=%d appState=%d\n", settings.mode, appState);
    if (appState == APP_MODE_SELECT) {
        menu_confirm();
        return;
    }
    switch (settings.mode) {
        case MODE_CUELIST:  action_cuelist_long();   break;
        case MODE_SNAP:     action_snap_long();      break;
        case MODE_OVERRIDE: 
            Serial.println("[MODE] chiamata action_override_long");
            action_override_long();  
            break;
        default: break;
    }
}

void mode_do_vlong() {
    // Se si era in SNAP → release immediato al momento del menu
    if (settings.mode == MODE_SNAP && toolboxReachable() && WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin("http://" + String(settings.masterHost) + "/api/snap/release");
        http.setTimeout(2000);
        http.GET();
        http.end();
        Serial.println("[SNAP] Release automatico all'apertura menu");
    }

    appState    = APP_MODE_SELECT;
    pendingMode = settings.mode > 0 ? settings.mode : 1;
    matrix_show_menu(pendingMode);
    Serial.println("[MODE] Menu selezione modalità attivo");
}

// ==========================================================
// TALLY DISPLAY
// ==========================================================
void tally_display_update() {
    if (!toolboxStatus.valid) {
        // Nessuna connessione — A rossa lampeggiante
        static bool on = true;
        static uint32_t last = 0;
        if (millis() - last > 500) { on = !on; last = millis(); }
         if (on) mp_no_connection();
        else { matrix_clear(); matrix_show(); }
        return;
    }
        // PRIORITÀ 1
        if (toolboxStatus.blackout) { mp_blackout(); return; }

        // PRIORITÀ 2 — crossfade prima di running off
        if (toolboxStatus.crossfade) { mp_crossfade_tick(); return; }

        if (!toolboxStatus.running) { mp_running_off(); return; }

        if (toolboxStatus.keypad) { mp_mode(2); return; }

        // PRIORITÀ 2 — artnet searching
        if (!toolboxStatus.artnet && toolboxStatus.mode == 1 && toolboxStatus.running) {
            mp_artnet_searching(); return;
        }



    // CICLO parametri abilitati per matrice
    // static uint8_t matIdx = 0;
    static uint32_t lastSwitch = 0;
    uint32_t interval = (uint32_t)settings.tallyInterval * 1000UL;

        if (settings.tallyAutoplay && millis() - lastSwitch > interval) {
            tallyIdx = (tallyIdx + 1) % max((uint8_t)1, settings.tallyCount);
            lastSwitch = millis();
        }

        const char* param = settings.tallyParams[tallyIdx % max((uint8_t)1, settings.tallyCount)];

            if (strcmp(param, "running") == 0) {
            mp_running_on();
        } else if (strcmp(param, "scene") == 0) {
            if (toolboxStatus.scene) {
                mp_scene(toolboxStatus.snapId);
            } else {
                // salta — avanza al prossimo parametro
                tallyIdx = (tallyIdx + 1) % max((uint8_t)1, settings.tallyCount);
            }
        } else if (strcmp(param, "universe") == 0) {
            mp_universe(toolboxStatus.universe);
        } else if (strcmp(param, "mode") == 0) {
            mp_mode(toolboxStatus.mode);
        }
}

// ==========================================================
// INIT E LOOP
// ==========================================================
void mode_init() {
    pendingMode = settings.mode > 0 ? settings.mode : 1;
    matrix_show_mode(settings.mode);
}

void mode_loop() {
    BtnEvent ev = btn_read();
    if (ev == BTN_SHORT) mode_do_short();
    if (ev == BTN_LONG)  mode_do_long();
    if (ev == BTN_VLONG) mode_do_vlong();

    if (millis() - lastPoll > 2000) {
        lastPoll = millis();
        pollToolbox();
    }

    if (appState == APP_NORMAL) {
        if (settings.mode == MODE_TALLY) {
            tally_display_update();
        } else if (settings.mode == MODE_SNAP) {
            // Mostra snap corrente sulla matrice
            static uint32_t lastRefresh = 0;
            if (snapCount > 0 && millis() - lastRefresh > 100) {
                lastRefresh = millis();
                mp_snap_index(snapIds[snapCurrentIdx]);
            }
        } else {
            static uint32_t lastRefresh = 0;
            if (millis() - lastRefresh > 5000) {
                lastRefresh = millis();
                matrix_show_mode(settings.mode);
            }
        }
    }
}

void load_snap_list() {
    if (!toolboxReachable()) return;
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    String url = "http://" + String(settings.masterHost) + "/api/snapshots";
    http.begin(url);
    http.setTimeout(2000);
    int code = http.GET();

    if (code == 200) {
        String body = http.getString();
        StaticJsonDocument<1024> doc;
        if (deserializeJson(doc, body) == DeserializationError::Ok) {
            JsonArray arr = doc.as<JsonArray>();
            snapCount = 0;
            for (JsonObject s : arr) {
                if (snapCount >= 50) break;
                snapIds[snapCount] = s["id"] | 0;
                strlcpy(snapNames[snapCount], s["name"] | "", 32);
                snapCount++;
            }
            snapCurrentIdx = 0;
            snapLoaded = true;
            Serial.printf("[SNAP] Caricati %d snap\n", snapCount);
        }
    }
    http.end();
}