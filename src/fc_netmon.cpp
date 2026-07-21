#include "fc_netmon.h"
#include "fc_config.h"
#include "fc_mood.h"
#include <WiFi.h>
#include "ping/ping_sock.h"     // ESP-IDF native ICMP; no external library needed
#include "lwip/ip_addr.h"
#include "lwip/inet.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

/* -------------------------------- state --------------------------------- */

static SemaphoreHandle_t mtx;
static int   s_rssi = 0;
static bool  s_up = false;
static int   s_latency = -1;
static uint8_t s_hist = 0;        // last 8 internet checks as bit history (1=ok)
static std::vector<NetDevice> s_devices;
static std::vector<WifiAP>    s_scan;
static volatile bool s_scanReq = false;
static volatile bool s_scanBusy = false;
static bool s_wasUp = true;       // for outage/recovery edge detection
static volatile int s_moodEvent = 0;  // 0=none, 1=outage, 2=recovery (consumed in loop)

#define LOCK()   xSemaphoreTake(mtx, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(mtx)

/* ------------------------------- helpers -------------------------------- */

static int qualityFromRssi(int rssi) {
    if (rssi == 0) return 0;
    if (rssi <= -100) return 0;
    if (rssi >= -50) return 100;
    return 2 * (rssi + 100);
}

static void parseDevices(std::vector<NetDevice>& out) {
    out.clear();
    String s = cfg.devices;
    int start = 0;
    while (start < (int)s.length() && out.size() < 8) {
        int comma = s.indexOf(',', start);
        String tok = (comma < 0) ? s.substring(start) : s.substring(start, comma);
        tok.trim();
        int colon = tok.indexOf(':');
        if (colon > 0) {
            NetDevice d;
            d.name = tok.substring(0, colon); d.name.trim();
            d.ip   = tok.substring(colon + 1); d.ip.trim();
            d.online = false; d.rtt = -1;
            if (d.ip.length()) out.push_back(d);
        }
        if (comma < 0) break;
        start = comma + 1;
    }
}

/* ------------------------------ ICMP ping -------------------------------- */
// Uses the ping stack built into ESP-IDF rather than a third-party library, so
// there is nothing extra to keep compatible with future Arduino-ESP32 cores.

struct PingCtx {
    SemaphoreHandle_t done;
    volatile uint32_t replies;
    volatile uint32_t rtt;
};

static void pingOnSuccess(esp_ping_handle_t hdl, void* args) {
    PingCtx* ctx = (PingCtx*)args;
    uint32_t elapsed = 0;
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed, sizeof(elapsed));
    ctx->replies++;
    ctx->rtt = elapsed;
}

static void pingOnEnd(esp_ping_handle_t hdl, void* args) {
    PingCtx* ctx = (PingCtx*)args;
    xSemaphoreGive(ctx->done);
}

// Returns true if at least one echo reply came back; rttOut gets the last RTT.
static bool icmpPing(const String& host, uint8_t count, uint32_t timeoutMs, uint32_t& rttOut) {
    IPAddress ip;
    if (!ip.fromString(host.c_str())) {
        if (!WiFi.hostByName(host.c_str(), ip)) return false;   // resolve names
    }

    ip_addr_t target;
    memset(&target, 0, sizeof(target));
    IP_ADDR4(&target, ip[0], ip[1], ip[2], ip[3]);

    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.target_addr    = target;
    config.count          = count;
    config.timeout_ms     = timeoutMs;
    config.interval_ms    = 200;
    config.task_stack_size = 4096;

    PingCtx ctx;
    ctx.done    = xSemaphoreCreateBinary();
    ctx.replies = 0;
    ctx.rtt     = 0;
    if (!ctx.done) return false;

    esp_ping_callbacks_t cbs;
    memset(&cbs, 0, sizeof(cbs));
    cbs.cb_args         = &ctx;
    cbs.on_ping_success = pingOnSuccess;
    cbs.on_ping_end     = pingOnEnd;

    esp_ping_handle_t hdl = nullptr;
    if (esp_ping_new_session(&config, &cbs, &hdl) != ESP_OK || !hdl) {
        vSemaphoreDelete(ctx.done);
        return false;
    }

    esp_ping_start(hdl);
    // generous ceiling so we always reclaim the session even if it stalls
    uint32_t budget = (uint32_t)count * (timeoutMs + config.interval_ms) + 1500;
    xSemaphoreTake(ctx.done, pdMS_TO_TICKS(budget));
    esp_ping_stop(hdl);
    esp_ping_delete_session(hdl);
    vSemaphoreDelete(ctx.done);

    rttOut = ctx.rtt;
    return ctx.replies > 0;
}

/* ----------------------------- worker task ------------------------------ */

static void netTask(void*) {
    for (;;) {
        // handle a requested AP scan first (blocks briefly)
        if (s_scanReq) {
            s_scanReq = false;
            s_scanBusy = true;
            int n = WiFi.scanNetworks(false, true);   // sync, show hidden
            std::vector<WifiAP> found;
            for (int i = 0; i < n && i < 16; i++) {
                found.push_back({WiFi.SSID(i), WiFi.RSSI(i), WiFi.channel(i)});
            }
            WiFi.scanDelete();
            LOCK(); s_scan = found; UNLOCK();
            s_scanBusy = false;
        }

        bool connected = WiFi.status() == WL_CONNECTED;
        int rssi = connected ? WiFi.RSSI() : 0;

        // internet reachability + latency (ICMP to configured host)
        bool up = false; int lat = -1;
        if (connected) {
            uint32_t rtt = 0;
            if (icmpPing(cfg.ping_host, 2, 1000, rtt)) { up = true; lat = (int)rtt; }
        }

        // presence: ping each tracked device once
        std::vector<NetDevice> devs;
        parseDevices(devs);
        if (connected) {
            for (auto& d : devs) {
                uint32_t rtt = 0;
                if (icmpPing(d.ip, 1, 800, rtt)) { d.online = true; d.rtt = (int)rtt; }
            }
        }

        // commit snapshot
        LOCK();
        s_rssi = rssi;
        s_up = up;
        s_latency = lat;
        s_hist = (s_hist << 1) | (up ? 1 : 0);
        s_devices = devs;
        UNLOCK();

        // mood edge reactions (only from Face/neutral so we don't stomp chats)
        if (connected) {
            if (s_wasUp && !up)       s_moodEvent = 1;   // just went down
            else if (!s_wasUp && up)  s_moodEvent = 2;   // just recovered
            s_wasUp = up;
        }

        vTaskDelay(pdMS_TO_TICKS(8000));
    }
}

/* -------------------------------- public -------------------------------- */

void netmonInit() {
    mtx = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(netTask, "fc_net", 6144, nullptr, 1, nullptr, 0);
}

void netmonUpdate() {
    int ev = s_moodEvent;
    if (!ev) return;
    s_moodEvent = 0;
    if (ev == 1) { moodSet(Mood::Sad, 6000);  moodChirp(Mood::Sad); }
    else         { moodSet(Mood::Happy, 4000); moodChirp(Mood::Happy); }
}

int  netRssi()        { LOCK(); int v = s_rssi; UNLOCK(); return v; }
int  netQuality()     { return qualityFromRssi(netRssi()); }
bool netInternetUp()  { LOCK(); bool v = s_up; UNLOCK(); return v; }
int  netLatencyMs()   { LOCK(); int v = s_latency; UNLOCK(); return v; }

int netLossPct() {
    LOCK(); uint8_t h = s_hist; UNLOCK();
    int ok = 0; for (int i = 0; i < 8; i++) ok += (h >> i) & 1;
    return (8 - ok) * 100 / 8;
}

String netSummaryLine() {
    if (WiFi.status() != WL_CONNECTED) return "Offline";
    String s = String(netQuality()) + "% signal";
    if (netInternetUp()) s += ", net " + String(netLatencyMs()) + "ms";
    else                 s += ", NET DOWN";
    return s;
}

std::vector<NetDevice> netDevices()   { LOCK(); auto v = s_devices; UNLOCK(); return v; }
std::vector<WifiAP>    netScanResults(){ LOCK(); auto v = s_scan; UNLOCK(); return v; }
void netRequestScan()  { s_scanReq = true; }
bool netScanBusy()     { return s_scanBusy; }

String netJson() {
    String j = "{";
    j += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
    j += ",\"ssid\":\"" + WiFi.SSID() + "\"";
    j += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
    j += ",\"rssi\":" + String(netRssi());
    j += ",\"quality\":" + String(netQuality());
    j += ",\"internet_up\":" + String(netInternetUp() ? "true" : "false");
    j += ",\"latency_ms\":" + String(netLatencyMs());
    j += ",\"loss_pct\":" + String(netLossPct());
    j += ",\"devices\":[";
    auto devs = netDevices();
    for (size_t i = 0; i < devs.size(); i++) {
        if (i) j += ",";
        j += "{\"name\":\"" + devs[i].name + "\",\"ip\":\"" + devs[i].ip +
             "\",\"online\":" + String(devs[i].online ? "true" : "false") +
             ",\"rtt\":" + String(devs[i].rtt) + "}";
    }
    j += "],\"scan\":[";
    auto aps = netScanResults();
    for (size_t i = 0; i < aps.size(); i++) {
        if (i) j += ",";
        String ss = aps[i].ssid; ss.replace("\"", "'");
        j += "{\"ssid\":\"" + ss + "\",\"rssi\":" + String(aps[i].rssi) +
             ",\"ch\":" + String(aps[i].channel) + "}";
    }
    j += "]}";
    return j;
}
