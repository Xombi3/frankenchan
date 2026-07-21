#include "fc_espnow.h"
#include "fc_config.h"
#include "fc_mood.h"
#include "fc_motion.h"
#include "fc_action.h"
#include "fc_mqtt.h"
#include <M5StackChan.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Wire format shared with the sensor nodes (keep in sync with extras/sensor_node)
typedef struct __attribute__((packed)) {
    char     magic[4];    // "FCSN"
    char     id[16];
    char     kind[12];
    float    value;
    int8_t   bearing;     // where this sensor is, in degrees, relative to home
    uint8_t  alert;       // 1 = worth reacting to out loud
    char     text[32];
} fc_sensor_packet_t;

static std::vector<SensorNode> nodes;
static QueueHandle_t rxq = nullptr;

typedef struct { fc_sensor_packet_t pkt; int rssi; } rx_item_t;

static void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (len != sizeof(fc_sensor_packet_t) || !rxq) return;
    rx_item_t item;
    memcpy(&item.pkt, data, sizeof(item.pkt));
    if (memcmp(item.pkt.magic, "FCSN", 4) != 0) return;
    item.rssi = info && info->rx_ctrl ? info->rx_ctrl->rssi : 0;
    xQueueSend(rxq, &item, 0);   // handled on the main thread
}

void espnowInit() {
    rxq = xQueueCreate(8, sizeof(rx_item_t));
    // ESP-NOW rides alongside the normal STA connection; it must share the channel.
    if (WiFi.getMode() == WIFI_OFF) WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) return;
    esp_now_register_recv_cb(onRecv);
}

static SensorNode* findNode(const String& id) {
    for (auto& n : nodes) if (n.id == id) return &n;
    return nullptr;
}

void espnowUpdate() {
    if (!rxq) return;
    rx_item_t item;
    while (xQueueReceive(rxq, &item, 0) == pdTRUE) {
        String id = String(item.pkt.id);
        if (!id.length()) continue;

        SensorNode* n = findNode(id);
        if (!n) {
            if (nodes.size() >= 12) continue;
            nodes.push_back(SensorNode());
            n = &nodes.back();
            n->id = id;
        }
        n->kind     = String(item.pkt.kind);
        n->value    = item.pkt.value;
        n->text     = String(item.pkt.text);
        n->bearing  = item.pkt.bearing;
        n->lastSeen = millis();
        n->rssi     = item.rssi;

        mqttPublishEvent("sensor", n->id + "=" + String(n->value, 2));
        if (item.pkt.alert) {
            // physically turn toward whatever just happened, then announce it
            gestureLookBearing(item.pkt.bearing);
            moodSet(Mood::Doubt, 6000);
            moodChirp(Mood::Doubt);
            if (cfg.espnow_speak && n->text.length()) {
                runAction("say:" + n->text);
            }
        }
    }
}

std::vector<SensorNode> espnowNodes() { return nodes; }

String espnowJson() {
    String j = "{\"nodes\":[";
    for (size_t i = 0; i < nodes.size(); i++) {
        if (i) j += ",";
        j += "{\"id\":\"" + nodes[i].id + "\",\"kind\":\"" + nodes[i].kind +
             "\",\"value\":" + String(nodes[i].value, 2) +
             ",\"text\":\"" + nodes[i].text + "\"" +
             ",\"rssi\":" + String(nodes[i].rssi) +
             ",\"age_s\":" + String((millis() - nodes[i].lastSeen) / 1000) + "}";
    }
    j += "]}";
    return j;
}
