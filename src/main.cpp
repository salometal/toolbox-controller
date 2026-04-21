#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <FastLED.h>
#include "config.h"
#include "net/network_engine.h"
#include "hw/hw_manager.h"
#include "core/mode_engine.h"

// --- GLOBALI ---
AsyncWebServer server(80);
AtomConfig settings;
CRGB matrix[MATRIX_NUM];
String wifiScanResults = "";

void saveConfiguration() {
    File f = LittleFS.open("/config.bin", "w");
    if (f) {
        f.write((const uint8_t*)&settings, sizeof(AtomConfig));
        f.close();
        Serial.println("[FS] Config salvata.");
    }
}

void loadConfiguration() {
    if (LittleFS.exists("/config.bin")) {
        File f = LittleFS.open("/config.bin", "r");
        if (f) {
            f.read((uint8_t*)&settings, sizeof(AtomConfig));
            f.close();
            Serial.println("[FS] Config caricata.");
        }
    } else {
        Serial.println("[FS] Prima accensione — default.");
        memset(&settings, 0, sizeof(AtomConfig));
        strlcpy(settings.hostname, "atomcompanion", sizeof(settings.hostname));
        settings.brightness    = 40;
        settings.mode          = MODE_NONE;
        settings.use_dhcp      = 1;
        settings.tallyInterval = 5;
        settings.tallyCount    = 0;
        settings.activeListIndex = 0;
        saveConfiguration();
    }
    // TEMP: forza connessione WiFi
    strlcpy(settings.ssid, "TIM-Salo", sizeof(settings.ssid));
    strlcpy(settings.pass, "comAppy01", sizeof(settings.pass));
    settings.use_dhcp = 1; 
}

void initWiFi() {
    if (strlen(settings.ssid) == 0) {
        Serial.println("[NET] Nessun SSID — avvio AP");
        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(IPAddress(192,168,1,1), IPAddress(192,168,1,1), IPAddress(255,255,255,0));
        WiFi.softAP("ATOM-Companion", "12345678");
        Serial.print("[NET] AP IP: ");
        Serial.println(WiFi.softAPIP());
        return;
    }

    Serial.printf("[NET] Connessione a: %s\n", settings.ssid);

    if (!settings.use_dhcp) {
        WiFi.config(
            IPAddress(settings.ip[0], settings.ip[1], settings.ip[2], settings.ip[3]),
            IPAddress(settings.gateway[0], settings.gateway[1], settings.gateway[2], settings.gateway[3]),
            IPAddress(settings.subnet[0], settings.subnet[1], settings.subnet[2], settings.subnet[3])
        );
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(settings.ssid, settings.pass);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[NET] Connesso! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n[NET] Timeout — avvio AP fallback");
        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(IPAddress(192,168,1,1), IPAddress(192,168,1,1), IPAddress(255,255,255,0));
        WiFi.softAP("Toolbox-controller", "12345678");
    }
}

void setupMDNS() {
    if (strlen(settings.hostname) < 3)
        strlcpy(settings.hostname, "atomcompanion", sizeof(settings.hostname));

    if (MDNS.begin(settings.hostname)) {
        Serial.printf("[mDNS] http://%s.local\n", settings.hostname);
        MDNS.addService("http", "tcp", 80);
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);

    // LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] Errore mount LittleFS!");
    }

    // Config
    loadConfiguration();

    // Brightness sicura
    if (settings.brightness < 5 || settings.brightness > 255)
        settings.brightness = 40;

    // Hardware
    hw_init();

    // WiFi scan prima di connettersi
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    int n = WiFi.scanNetworks();
    wifiScanResults = "";
    for (int i = 0; i < n; i++) {
        wifiScanResults += WiFi.SSID(i) + "|" + String(WiFi.RSSI(i));
        if (i < n - 1) wifiScanResults += ",";
    }
    WiFi.scanDelete();
Serial.println("[SCAN] Risultati: " + wifiScanResults);
    // WiFi + mDNS
    initWiFi();
    setupMDNS();

    // Web server
    setupWebServer();

    // Boot animation
    hw_boot();

    Serial.println("--- ATOM COMPANION PRONTO ---");
}

void loop() {
    hw_loop();
    mode_loop();
    delay(10);
}