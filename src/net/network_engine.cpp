#include "network_engine.h"
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "../hw/hw_manager.h"

extern AsyncWebServer server;
extern AtomConfig settings;
extern String wifiScanResults;

ToolboxStatus toolboxStatus = {};

// ==========================================================
// POLLING TOOLBOX
// ==========================================================
bool toolboxReachable() {
    return strlen(settings.masterHost) > 0;
}

void pollToolbox() {
    if (!toolboxReachable()) return;
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    String url = "http://" + String(settings.masterHost) + "/api/status";
    http.begin(url);
    http.setTimeout(2000);

    int code = http.GET();
    if (code == 200) {
        String body = http.getString();
        StaticJsonDocument<512> doc;
        if (deserializeJson(doc, body) == DeserializationError::Ok) {
            toolboxStatus.running   = doc["running"]  | false;
            toolboxStatus.mode      = doc["mode"]     | 0;
            toolboxStatus.blackout  = doc["blackout"] | false;
            toolboxStatus.scene     = doc["scene"]    | false;
            toolboxStatus.artnet    = doc["artnet"]   | false;
            toolboxStatus.crossfade = doc["crossfade"]| false;
            toolboxStatus.universe  = doc["universe"] | 0;
            toolboxStatus.refresh   = doc["refresh"]  | 0;
            toolboxStatus.heap      = doc["heap"]     | 0;
            toolboxStatus.uptime    = doc["uptime"]   | 0;
            strlcpy(toolboxStatus.hostname,
                    doc["hostname"] | "", sizeof(toolboxStatus.hostname));
            toolboxStatus.valid = true;
            toolboxStatus.snapId = doc["snapId"] | -1;
            toolboxStatus.keypad = doc["keypad"] | false;
        }
    } else {
        toolboxStatus.valid = false;
    }
    http.end();
}

// ==========================================================
// SCAN mDNS TOOLBOX
// ==========================================================
String scanToolboxMDNS() {
    int n = MDNS.queryService("dmxtoolbox", "tcp");
    String json = "[";
    bool first = true;

    for (int i = 0; i < n; i++) {
        // Leggi TXT record type
        String type = MDNS.txt(i, "type");
        if (type != "pro") continue; // filtra solo type=pro

        if (!first) json += ",";
        first = false;

        String hostname = MDNS.hostname(i);
        String ip = MDNS.IP(i).toString();
        String version = MDNS.txt(i, "version");
        String caps = MDNS.txt(i, "caps");

        json += "{\"name\":\"" + hostname +
                "\",\"host\":\"" + hostname + ".local" +
                "\",\"ip\":\"" + ip +
                "\",\"version\":\"" + version +
                "\",\"caps\":\"" + caps + "\"}";

        Serial.printf("[SCAN] Trovato PRO: %s (%s) v%s\n",
                      hostname.c_str(), ip.c_str(), version.c_str());
    }
    json += "]";
    return json;
}

// ==========================================================
// WEB SERVER
// ==========================================================
void setupWebServer() {

    // --- ROOT ---
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/index.html"))
            request->send(LittleFS, "/index.html", "text/html");
        else
            request->send(404, "text/plain", "index.html mancante");
    });

    // --- STATUS ATOM ---
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<256> doc;
        bool wifiOk = (WiFi.status() == WL_CONNECTED);
        doc["wifi"]            = wifiOk ? WiFi.SSID() : "AP";
        doc["ip"]              = wifiOk ? WiFi.localIP().toString()
                                        : WiFi.softAPIP().toString();
        doc["heap"]            = (int)(ESP.getFreeHeap() / 1024);
        doc["uptime"]          = (uint32_t)(millis() / 1000);
        doc["mode"]            = settings.mode;
        doc["masterName"]      = settings.masterName;
        doc["masterConnected"] = toolboxStatus.valid;
        doc["hostname"]        = settings.hostname;

        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    // --- WIFI SCAN ---
    server.on("/wifi_scan", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", wifiScanResults);
    });

    // --- CONNECT (salva e riavvia) ---
    server.on("/connect", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("s"))
            strlcpy(settings.ssid, request->getParam("s")->value().c_str(), sizeof(settings.ssid));
        if (request->hasParam("p"))
            strlcpy(settings.pass, request->getParam("p")->value().c_str(), sizeof(settings.pass));
        if (request->hasParam("h"))
            strlcpy(settings.hostname, request->getParam("h")->value().c_str(), sizeof(settings.hostname));

        if (request->hasParam("dhcp"))
            settings.use_dhcp = request->getParam("dhcp")->value().toInt();

        auto parseIP = [&](const char* param, uint8_t* target) {
            if (request->hasParam(param)) {
                IPAddress ip;
                if (ip.fromString(request->getParam(param)->value()))
                    for (int i = 0; i < 4; i++) target[i] = ip[i];
            }
        };
        if (!settings.use_dhcp) {
            parseIP("ip", settings.ip);
            parseIP("gw", settings.gateway);
            parseIP("sn", settings.subnet);
        }

        saveConfiguration();
        request->send(200, "text/plain", "Riavvio in corso...");
        delay(1500);
        ESP.restart();
    });

    // --- API CONFIG GET ---
    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<512> doc;
        doc["mode"]       = settings.mode;
        doc["masterHost"] = settings.masterHost;
        doc["masterName"] = settings.masterName;
        doc["brightness"] = settings.brightness;
        doc["hostname"]   = settings.hostname;
        doc["snapFade"] = settings.snapFade;

        JsonObject tally = doc.createNestedObject("tallyConfig");
        JsonArray params = tally.createNestedArray("params");
        for (int i = 0; i < settings.tallyCount; i++)
            params.add(settings.tallyParams[i]);
        tally["autoplay"] = settings.tallyAutoplay;
        tally["interval"] = settings.tallyInterval;

        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    // --- API CONFIG POST ---
    server.on("/api/config", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            StaticJsonDocument<512> doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"error\":\"json invalido\"}");
                return;
            }

            if (doc.containsKey("mode"))
                settings.mode = doc["mode"];
            if (doc.containsKey("masterHost"))
                strlcpy(settings.masterHost, doc["masterHost"] | "", sizeof(settings.masterHost));
            if (doc.containsKey("masterName"))
                strlcpy(settings.masterName, doc["masterName"] | "", sizeof(settings.masterName));
            if (doc.containsKey("brightness")) {
                settings.brightness = doc["brightness"];
                matrix_brightness(settings.brightness);
            }

            if (doc.containsKey("tallyConfig")) {
                JsonObject tally = doc["tallyConfig"];
                settings.tallyAutoplay = tally["autoplay"] | false;
                settings.tallyInterval = tally["interval"] | 5;
                JsonArray params = tally["params"];
                settings.tallyCount = 0;
                for (JsonVariant v : params) {
                    if (settings.tallyCount >= 8) break;
                    strlcpy(settings.tallyParams[settings.tallyCount++],
                            v.as<const char*>(), 16);
                }
            }
            if (doc.containsKey("snapFade"))
    settings.snapFade = doc["snapFade"] | 0.0f;

            saveConfiguration();
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // --- SCAN TOOLBOX mDNS ---
    server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", scanToolboxMDNS());
    });

    // --- API BRIGHTNESS ---
    server.on("/api/brightness", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("val")) {
            settings.brightness = constrain(
                request->getParam("val")->value().toInt(), 5, 255);
            matrix_brightness(settings.brightness);
            saveConfiguration();
        }
        request->send(200, "text/plain", "OK");
    });

    // --- IDENTIFY ---
    server.on("/identify", HTTP_GET, [](AsyncWebServerRequest *request) {
        matrix_identify();
        request->send(200, "text/plain", "OK");
    });

    // --- RESTART ---
    server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Riavvio...");
        delay(500);
        ESP.restart();
    });
        // --- CUELIST GET ---
        server.on("/api/cuelist", HTTP_GET, [](AsyncWebServerRequest *request) {
            if (LittleFS.exists("/cuelist.json"))
                request->send(LittleFS, "/cuelist.json", "application/json");
            else
                request->send(200, "application/json", "[]");
        });

        // --- CUELIST POST ---
        server.on("/api/cuelist", HTTP_POST,
            [](AsyncWebServerRequest *request) {},
            NULL,
            [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                File f = LittleFS.open("/cuelist.json", "w");
                if (f) { f.write(data, len); f.close(); }
                request->send(200, "application/json", "{\"ok\":true}");
            }
        );
    // --- FACTORY RESET ---
    server.on("/factoryreset", HTTP_GET, [](AsyncWebServerRequest *request) {
        memset(&settings, 0, sizeof(AtomConfig));
        strlcpy(settings.hostname, "atomcompanion", sizeof(settings.hostname));
        settings.brightness = 40;
        settings.use_dhcp   = 1;
        saveConfiguration();
        request->send(200, "text/plain", "Reset effettuato. Riavvio...");
        delay(1500);
        ESP.restart();
    });


    // --- STATIC FILES ---
    server.serveStatic("/", LittleFS, "/");

    server.begin();
    Serial.println("[WEB] Server avviato.");
}