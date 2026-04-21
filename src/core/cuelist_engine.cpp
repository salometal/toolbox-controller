#include "cuelist_engine.h"
#include "../net/network_engine.h"
#include "../hw/hw_manager.h"
#include <HTTPClient.h>

extern AtomConfig settings;
extern ToolboxStatus toolboxStatus;

int     cl_activeList  = 0;
int     cl_currentCue  = 0;
bool    cl_running     = false;
CueList cl_lists[8];
uint8_t cl_listCount   = 0;

static uint32_t autogoStart = 0;
static bool     autogoActive = false;

// ==========================================================
// LOAD / SAVE da LittleFS
// ==========================================================
void cuelist_save() {
    File f = LittleFS.open("/cuelist.json", "w");
    if (!f) return;

    StaticJsonDocument<8192> doc;
    doc["activeList"] = cl_activeList;
    JsonArray lists = doc.createNestedArray("lists");

    for (int i = 0; i < cl_listCount; i++) {
        JsonObject list = lists.createNestedObject();
        list["name"]  = cl_lists[i].name;
        list["count"] = cl_lists[i].count;
        JsonArray cues = list.createNestedArray("cues");
        for (int j = 0; j < cl_lists[i].count; j++) {
            JsonObject cue = cues.createNestedObject();
            cue["name"]       = cl_lists[i].cues[j].name;
            cue["snapId"]     = cl_lists[i].cues[j].snapId;
            cue["fade"]       = cl_lists[i].cues[j].fade;
            cue["goMode"]     = cl_lists[i].cues[j].goMode;
            cue["autogoDelay"]= cl_lists[i].cues[j].autogoDelay;
            cue["color"][0]   = cl_lists[i].cues[j].color[0];
            cue["color"][1]   = cl_lists[i].cues[j].color[1];
            cue["color"][2]   = cl_lists[i].cues[j].color[2];
        }
    }

    serializeJson(doc, f);
    f.close();
    Serial.println("[CL] Salvato cuelist.json");
}

void cuelist_load(int listIndex) {
    if (!LittleFS.exists("/cuelist.json")) return;

    File f = LittleFS.open("/cuelist.json", "r");
    if (!f) return;

    StaticJsonDocument<8192> doc;
    if (deserializeJson(doc, f) != DeserializationError::Ok) {
        f.close();
        return;
    }
    f.close();

    cl_listCount = 0;
    JsonArray lists = doc["lists"];
    for (JsonObject list : lists) {
        if (cl_listCount >= 8) break;
        CueList& cl = cl_lists[cl_listCount];
        strlcpy(cl.name, list["name"] | "", sizeof(cl.name));
        cl.count = 0;
        JsonArray cues = list["cues"];
        for (JsonObject cue : cues) {
            if (cl.count >= 32) break;
            Cue& c = cl.cues[cl.count];
            strlcpy(c.name,   cue["name"]   | "", sizeof(c.name));
            strlcpy(c.snapId, cue["snapId"] | "", sizeof(c.snapId));
            c.fade        = cue["fade"]        | 0.0f;
            c.goMode      = cue["goMode"]      | 0;
            c.autogoDelay = cue["autogoDelay"] | 5.0f;
            c.color[0]    = cue["color"][0]    | 100;
            c.color[1]    = cue["color"][1]    | 100;
            c.color[2]    = cue["color"][2]    | 100;
            cl.count++;
        }
        cl_listCount++;
    }

    cl_activeList = listIndex < cl_listCount ? listIndex : 0;
    cl_currentCue = 0;
    Serial.printf("[CL] Caricate %d liste\n", cl_listCount);
}

// ==========================================================
// RECALL CUE SUL TOOLBOX
// ==========================================================
static void recall_current_cue() {
    if (cl_listCount == 0) return;
    CueList& list = cl_lists[cl_activeList];
    if (list.count == 0) return;
    Cue& cue = list.cues[cl_currentCue];

    if (!toolboxReachable()) return;

    String snapId = String(cue.snapId);
    if (snapId.length() == 0) return;

    HTTPClient http;
    String url = "http://" + String(settings.masterHost) +
                 "/api/snap/recall?id=" + snapId +
                 "&fade=" + String(cue.fade, 1);
    http.begin(url);
    http.setTimeout(3000);
    int code = http.GET();
    http.end();

    Serial.printf("[CL] Recall cue %d snap=%s fade=%.1f code=%d\n",
                  cl_currentCue, cue.snapId, cue.fade, code);

    // Feedback matrice
    CRGB c = CRGB(cue.color[0], cue.color[1], cue.color[2]);
    matrix_blink(c, 1, 200);
}

// ==========================================================
// GO / BACK
// ==========================================================
void cuelist_go() {
    if (cl_listCount == 0) return;
    CueList& list = cl_lists[cl_activeList];
    if (list.count == 0) return;

    recall_current_cue();

    // Autogo setup
    Cue& cue = list.cues[cl_currentCue];
    if (cue.goMode == 1) {
        autogoStart  = millis();
        autogoActive = true;
    } else {
        autogoActive = false;
    }

    // Avanza indice
    cl_currentCue = (cl_currentCue + 1) % list.count;
}

void cuelist_back() {
    if (cl_listCount == 0) return;
    CueList& list = cl_lists[cl_activeList];
    if (list.count == 0) return;

    autogoActive  = false;
    cl_currentCue = (cl_currentCue - 1 + list.count) % list.count;
    recall_current_cue();
}

// ==========================================================
// INIT E LOOP
// ==========================================================
void cuelist_init() {
    cuelist_load(0);
}

void cuelist_loop() {
    // Gestione autogo
    if (!autogoActive) return;
    if (cl_listCount == 0) return;

    CueList& list = cl_lists[cl_activeList];
    if (list.count == 0) return;

    // cl_currentCue è già avanzato dopo il GO — guarda la cue precedente
    int prevIdx = (cl_currentCue - 1 + list.count) % list.count;
    Cue& prev   = list.cues[prevIdx];

    if (millis() - autogoStart >= (uint32_t)(prev.autogoDelay * 1000)) {
        autogoActive = false;
        cuelist_go();
    }
}