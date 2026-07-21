#include "fc_apps.h"
#include "fc_config.h"
#include "fc_mood.h"
#include "fc_motion.h"
#include "fc_netmon.h"
#include "fc_sense.h"
#include <M5Unified.h>
#include <M5StackChan.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

using namespace m5avatar;

static Avatar* avatar = nullptr;
static FCMode mode = FCMode::Face;
static char bubble[96];
static uint32_t nextRefresh = 0;

// weather cache
static String wxText = "";
static uint32_t wxFetched = 0;

// timer
static uint32_t timerEndsAt = 0;   // millis deadline, 0 = off

void appsInit(Avatar* av) { avatar = av; }
FCMode appsMode() { return mode; }

void appsSetMode(FCMode m) {
    mode = m;
    nextRefresh = 0;
    if (m == FCMode::Network) netRequestScan();
}

static void setBubble(const String& s) {
    strncpy(bubble, s.c_str(), sizeof(bubble) - 1);
    bubble[sizeof(bubble) - 1] = 0;
    avatar->setSpeechText(bubble[0] ? bubble : "");
}

void appsNextMode(int dir) {
    int m = ((int)mode + dir + 5) % 5;
    mode = (FCMode)m;
    nextRefresh = 0;  // refresh immediately
    M5.Speaker.tone(900 + m * 200, 60);
    switch (mode) {
        case FCMode::Face:    setBubble(""); break;
        case FCMode::Clock:   setBubble("Clock!"); break;
        case FCMode::Weather: setBubble("Weather!"); break;
        case FCMode::Timer:   setBubble("Timer! Poke my screen"); break;
        case FCMode::Network: setBubble("Network!"); netRequestScan(); break;
    }
}

/* ------------------------------- weather -------------------------------- */

static const char* wxDesc(int code) {
    if (code == 0) return "clear";
    if (code <= 2) return "partly cloudy";
    if (code == 3) return "cloudy";
    if (code <= 48) return "foggy";
    if (code <= 57) return "drizzle";
    if (code <= 67) return "rainy";
    if (code <= 77) return "snowy";
    if (code <= 82) return "showers";
    if (code <= 86) return "snow showers";
    return "stormy";
}

static void fetchWeather() {
    if (WiFi.status() != WL_CONNECTED) { wxText = "No WiFi for weather"; return; }
    WiFiClientSecure client; client.setInsecure();
    HTTPClient http; http.setTimeout(10000);

    // geocode city once if no coordinates saved
    if (cfg.lat == 0 && cfg.lon == 0 && cfg.city.length()) {
        String url = "https://geocoding-api.open-meteo.com/v1/search?count=1&name=" + cfg.city;
        url.replace(" ", "+");
        http.begin(client, url);
        if (http.GET() == 200) {
            JsonDocument d;
            if (!deserializeJson(d, http.getString()) && d["results"][0]["latitude"].is<float>()) {
                cfg.lat = d["results"][0]["latitude"].as<float>();
                cfg.lon = d["results"][0]["longitude"].as<float>();
                cfg.save();
            }
        }
        http.end();
    }
    if (cfg.lat == 0 && cfg.lon == 0) { wxText = "Set my city first!"; return; }

    String url = "https://api.open-meteo.com/v1/forecast?current_weather=true&latitude=" +
                 String(cfg.lat, 4) + "&longitude=" + String(cfg.lon, 4);
    http.begin(client, url);
    if (http.GET() == 200) {
        JsonDocument d;
        if (!deserializeJson(d, http.getString())) {
            float t = d["current_weather"]["temperature"] | -99.0f;
            int code = d["current_weather"]["weathercode"] | 0;
            wxText = String(t, 0) + "C, " + wxDesc(code);
        }
    } else {
        wxText = "Weather is hiding...";
    }
    http.end();
    wxFetched = millis();
}

/* -------------------------------- timer --------------------------------- */

void appsTimerAdd(uint32_t secs) {
    uint32_t now = millis();
    uint32_t base = (timerEndsAt > now) ? timerEndsAt : now;
    timerEndsAt = base + secs * 1000UL;
    nextRefresh = 0;
    M5.Speaker.tone(1400, 50);
}

void appsTimerCancel() {
    timerEndsAt = 0;
    nextRefresh = 0;
    M5.Speaker.tone(500, 120);
}

static void timerRing() {
    timerEndsAt = 0;
    moodSet(Mood::Excited, 8000);
    setBubble("Time's up!!");
    gestureWiggle();
    for (int i = 0; i < 6; i++) {
        M5.Speaker.tone(i % 2 ? 1800 : 2400, 150);
        M5StackChan.showRgbColor(i % 2 ? 255 : 0, i % 2 ? 0 : 255, 40);
        delay(220);
    }
    M5StackChan.showRgbColor(60, 50, 10);
}

/* -------------------------------- update -------------------------------- */

void appsUpdate() {
    uint32_t now = millis();

    // timer fires in any mode
    if (timerEndsAt && now >= timerEndsAt) timerRing();

    if (now < nextRefresh) return;

    switch (mode) {
        case FCMode::Face:
            nextRefresh = now + 1000;
            break;
        case FCMode::Clock: {
            struct tm ti;
            if (getLocalTime(&ti, 100)) {
                char b[48];
                strftime(b, sizeof(b), "%H:%M  %a %d %b", &ti);
                setBubble(b);
            } else {
                setBubble("Clock needs WiFi...");
            }
            nextRefresh = now + 5000;
            break;
        }
        case FCMode::Weather:
            if (!wxFetched || now - wxFetched > 10 * 60 * 1000UL) {
                setBubble("Checking the sky...");
                fetchWeather();
            }
            setBubble(wxText);
            nextRefresh = now + 10000;
            break;
        case FCMode::Timer: {
            if (timerEndsAt) {
                uint32_t left = (timerEndsAt - now) / 1000;
                char b[40];
                snprintf(b, sizeof(b), "%lu:%02lu left", (unsigned long)(left / 60),
                         (unsigned long)(left % 60));
                setBubble(b);
            } else {
                setBubble("Tap: +1 min, hold: cancel");
            }
            nextRefresh = now + 1000;
            break;
        }
        case FCMode::Network: {
            // rotate through: summary -> devices -> nearby scan
            static uint8_t page = 0;
            String line;
            if (netScanBusy()) {
                line = "Scanning nearby...";
            } else if (page == 0) {
                line = netSummaryLine();
                if (netLossPct() > 0) line += " (" + String(netLossPct()) + "% loss)";
            } else if (page == 1) {
                auto devs = netDevices();
                if (devs.empty()) {
                    line = "No devices tracked";
                } else {
                    int on = 0; for (auto& d : devs) if (d.online) on++;
                    line = String(on) + "/" + String((int)devs.size()) + " devices home";
                    for (auto& d : devs) {
                        if (d.online) { line = d.name + " home " + String(d.rtt) + "ms"; break; }
                    }
                }
            } else if (page == 2) {
                auto aps = netScanResults();
                if (aps.empty()) line = "Swipe found no APs";
                else line = String((int)aps.size()) + " APs: " + aps[0].ssid +
                            " " + String(aps[0].rssi) + "dBm";
            } else {
                // my MAC — needed to pair ESP-NOW sensor nodes
                String mac = WiFi.macAddress();
                line = "MAC " + mac.substring(9);
                if (senseNear()) line += " (hi!)";
            }
            setBubble(line);
            page = (page + 1) % 4;
            nextRefresh = now + 3500;
            break;
        }
    }
}
