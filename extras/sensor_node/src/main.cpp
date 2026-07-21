/*
 * FrankenChan ESP-NOW sensor node — universal build
 * -------------------------------------------------
 * One binary works on any node: nothing is hard-coded. On first boot it opens a
 * Wi-Fi setup portal where you name the node, pick what it senses, and set which
 * direction StackChan should turn when it fires.
 *
 * It BROADCASTS, so it never needs StackChan's MAC address, and it sweeps Wi-Fi
 * channels when reporting so it doesn't need to know your router's channel either.
 *
 * Setup:  power it up -> join Wi-Fi "FC-Node-xxxx" (password: stackchan)
 *         -> the setup page opens -> save. Done.
 * To reconfigure later: the portal reopens for 30s on every boot.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

/* ------------------------------ wire format ------------------------------ */
// Must stay in sync with src/fc_espnow.cpp on the robot.
typedef struct __attribute__((packed)) {
    char     magic[4];      // "FCSN"
    char     id[16];
    char     kind[12];
    float    value;
    int8_t   bearing;
    uint8_t  alert;
    char     text[32];
} fc_sensor_packet_t;

static uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* -------------------------------- config -------------------------------- */

struct NodeCfg {
    String   id       = "";
    String   kind     = "door";     // door | motion | temp | soil | mailbox | button
    int8_t   bearing  = 0;          // degrees; robot turns this way on alert
    uint8_t  pin      = 1;          // sensor GPIO
    uint8_t  activeLow = 1;         // digital sensors: trigger when pin reads LOW
    uint16_t interval = 60;         // heartbeat seconds
    uint8_t  channel  = 0;          // 0 = sweep all channels
    String   textOn   = "";         // spoken when it triggers ("" = silent)
    String   textOff  = "";
    bool     configured = false;
} cfg;

static Preferences prefs;

static void cfgLoad() {
    prefs.begin("fcnode", true);
    cfg.id        = prefs.getString("id", "");
    cfg.kind      = prefs.getString("kind", cfg.kind);
    cfg.bearing   = prefs.getChar("bear", 0);
    cfg.pin       = prefs.getUChar("pin", cfg.pin);
    cfg.activeLow = prefs.getUChar("alow", 1);
    cfg.interval  = prefs.getUShort("int", cfg.interval);
    cfg.channel   = prefs.getUChar("ch", 0);
    cfg.textOn    = prefs.getString("ton", "");
    cfg.textOff   = prefs.getString("toff", "");
    cfg.configured = prefs.getBool("done", false);
    prefs.end();
    if (!cfg.id.length()) {
        char b[20];
        snprintf(b, sizeof(b), "node_%04X", (uint16_t)(ESP.getEfuseMac() & 0xFFFF));
        cfg.id = String(b);
    }
}

static void cfgSave() {
    prefs.begin("fcnode", false);
    prefs.putString("id", cfg.id);
    prefs.putString("kind", cfg.kind);
    prefs.putChar("bear", cfg.bearing);
    prefs.putUChar("pin", cfg.pin);
    prefs.putUChar("alow", cfg.activeLow);
    prefs.putUShort("int", cfg.interval);
    prefs.putUChar("ch", cfg.channel);
    prefs.putString("ton", cfg.textOn);
    prefs.putString("toff", cfg.textOff);
    prefs.putBool("done", true);
    prefs.end();
}

/* ------------------------------ transmitting ----------------------------- */

static bool espnowReady = false;

static void espnowStart() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(50);
    if (esp_now_init() != ESP_OK) return;
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BROADCAST, 6);
    peer.channel = 0;       // follow whatever channel we're currently on
    peer.encrypt = false;
    esp_now_add_peer(&peer);
    espnowReady = true;
}

static void sendOnChannel(const fc_sensor_packet_t& p, uint8_t ch) {
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    delay(4);
    esp_now_send(BROADCAST, (const uint8_t*)&p, sizeof(p));
    delay(12);
}

static void report(float value, bool alert, const String& text) {
    if (!espnowReady) return;
    fc_sensor_packet_t p = {};
    memcpy(p.magic, "FCSN", 4);
    strncpy(p.id, cfg.id.c_str(), sizeof(p.id) - 1);
    strncpy(p.kind, cfg.kind.c_str(), sizeof(p.kind) - 1);
    p.value   = value;
    p.bearing = cfg.bearing;
    p.alert   = alert ? 1 : 0;
    strncpy(p.text, text.c_str(), sizeof(p.text) - 1);

    if (cfg.channel > 0) {
        sendOnChannel(p, cfg.channel);
    } else {
        // Sweep: StackChan sits on the router's channel and we don't know which.
        // Sending on each costs a few hundred ms and removes all configuration.
        static const uint8_t chans[] = {1, 6, 11, 2, 3, 4, 5, 7, 8, 9, 10, 12, 13};
        for (uint8_t ch : chans) sendOnChannel(p, ch);
    }
}

/* ------------------------------ setup portal ----------------------------- */

static WebServer server(80);
static DNSServer dns;
static bool portalRunning = false;
static uint32_t portalUntil = 0;

static String page() {
    String sel_kind[6] = {"door", "motion", "temp", "soil", "mailbox", "button"};
    String opts;
    for (auto& k : sel_kind) {
        opts += "<option value=" + k + (cfg.kind == k ? " selected" : "") + ">" + k + "</option>";
    }
    return String(R"(<!DOCTYPE html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>FrankenChan Node</title><style>
body{font-family:system-ui;background:#14141f;color:#eee;max-width:460px;margin:auto;padding:16px}
h1{color:#7fd4ff;font-size:1.3em}label{font-size:.85em;color:#aaa}
input,select{width:100%;box-sizing:border-box;padding:8px;margin:4px 0 10px;border-radius:8px;border:1px solid #444;background:#1e1e2e;color:#eee}
button{background:#7fd4ff;color:#123;border:0;border-radius:10px;padding:12px;font-weight:bold;width:100%;font-size:1em}
.hint{color:#666;font-size:.78em;margin-top:-6px;margin-bottom:10px}</style></head><body>
<h1>&#128225; Sensor Node</h1><form method=POST action=/save>
<label>Node name</label><input name=id value=")" + cfg.id + R"(">
<label>What does it sense?</label><select name=kind>)" + opts + R"(</select>
<label>Direction from StackChan (-120 to 120&deg;)</label>
<input name=bear type=number min=-120 max=120 value=")" + String(cfg.bearing) + R"(">
<div class=hint>The robot turns this way when this sensor fires. 0 = straight ahead.</div>
<label>Sensor GPIO pin</label><input name=pin type=number min=0 max=48 value=")" + String(cfg.pin) + R"(">
<label><input type=checkbox name=alow value=1 )" + (cfg.activeLow ? "checked" : "") + R"( style=width:auto> Triggers when pin reads LOW</label>
<label>Heartbeat interval (seconds)</label><input name=int type=number min=10 max=3600 value=")" + String(cfg.interval) + R"(">
<label>Say when triggered (blank = silent)</label><input name=ton value=")" + cfg.textOn + R"(" placeholder="The front door opened">
<label>Say when it clears</label><input name=toff value=")" + cfg.textOff + R"(">
<label>Wi-Fi channel (0 = auto sweep)</label><input name=ch type=number min=0 max=13 value=")" + String(cfg.channel) + R"(">
<div class=hint>Leave 0 unless you know your router's channel and want longer battery life.</div>
<button type=submit>Save &amp; Start</button></form></body></html>)");
}

static void handleSave() {
    if (server.arg("id").length())   cfg.id = server.arg("id");
    if (server.arg("kind").length()) cfg.kind = server.arg("kind");
    cfg.bearing   = constrain(server.arg("bear").toInt(), -120, 120);
    cfg.pin       = constrain(server.arg("pin").toInt(), 0, 48);
    cfg.activeLow = server.arg("alow") == "1" ? 1 : 0;
    cfg.interval  = constrain(server.arg("int").toInt(), 10, 3600);
    cfg.channel   = constrain(server.arg("ch").toInt(), 0, 13);
    cfg.textOn    = server.arg("ton");
    cfg.textOff   = server.arg("toff");
    cfgSave();
    server.send(200, "text/html",
        "<html><body style='font-family:system-ui;background:#14141f;color:#7fd4ff;text-align:center;padding-top:40px'>"
        "<h1>Saved!</h1><p style='color:#aaa'>Node is live. Watch for it on StackChan's dashboard.</p></body></html>");
    delay(600);
    ESP.restart();
}

static void portalStart() {
    WiFi.mode(WIFI_AP);
    char ap[24];
    snprintf(ap, sizeof(ap), "FC-Node-%04X", (uint16_t)(ESP.getEfuseMac() & 0xFFFF));
    WiFi.softAP(ap, "stackchan");
    dns.start(53, "*", WiFi.softAPIP());
    server.on("/", []() { server.send(200, "text/html", page()); });
    server.on("/save", HTTP_POST, handleSave);
    server.onNotFound([]() { server.send(200, "text/html", page()); });
    server.begin();
    portalRunning = true;
    Serial.printf("Setup portal: join \"%s\" (password stackchan)\n", ap);
}

static void portalStop() {
    server.stop();
    dns.stop();
    portalRunning = false;
    WiFi.softAPdisconnect(true);
    espnowStart();
    Serial.println("Portal closed, sensor running");
}

/* --------------------------------- main ---------------------------------- */

void setup() {
    Serial.begin(115200);
    delay(200);
    cfgLoad();
    Serial.printf("FrankenChan node \"%s\" (%s)\n", cfg.id.c_str(), cfg.kind.c_str());

    portalStart();
    // unconfigured nodes wait for setup; configured ones give you 30s to change things
    portalUntil = cfg.configured ? millis() + 30000 : 0;
}

static float readSensor() {
    pinMode(cfg.pin, cfg.activeLow ? INPUT_PULLUP : INPUT);
    int v = digitalRead(cfg.pin);
    bool triggered = cfg.activeLow ? (v == LOW) : (v == HIGH);
    return triggered ? 1.0f : 0.0f;
}

void loop() {
    if (portalRunning) {
        dns.processNextRequest();
        server.handleClient();
        if (portalUntil && millis() > portalUntil) portalStop();
        return;
    }

    static float last = -999;
    static uint32_t nextBeat = 0;
    uint32_t now = millis();

    float v = readSensor();
    if (v != last) {
        bool on = v > 0.5f;
        last = v;
        report(v, true, on ? cfg.textOn : cfg.textOff);
        nextBeat = now + (uint32_t)cfg.interval * 1000UL;
    } else if (now >= nextBeat) {
        nextBeat = now + (uint32_t)cfg.interval * 1000UL;
        report(v, false, "");
    }
    delay(80);

    // ---- Battery tip -------------------------------------------------------
    // For slow-changing sensors, pin the Wi-Fi channel in the setup portal and
    // replace everything above with a deep-sleep cycle:
    //
    //   report(readSensor(), false, "");
    //   esp_sleep_enable_timer_wakeup((uint64_t)cfg.interval * 1000000ULL);
    //   esp_deep_sleep_start();
    //
    // For instant-reaction sensors (door, button), wake on the pin instead:
    //   esp_sleep_enable_ext0_wakeup((gpio_num_t)cfg.pin, cfg.activeLow ? 0 : 1);
}
