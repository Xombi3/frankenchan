#include "fc_mqtt.h"
#include "fc_config.h"
#include "fc_action.h"
#include "fc_mood.h"
#include "fc_netmon.h"
#include "fc_sense.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <M5StackChan.h>

static WiFiClient net;
static PubSubClient mq(net);
static String baseTopic;
static uint32_t nextState = 0;
static uint32_t nextRetry = 0;

static String topic(const char* leaf) { return baseTopic + "/" + leaf; }

/* ------------------------- Home Assistant discovery ---------------------- */

static void publishDiscovery() {
    if (!cfg.mqtt_ha_discovery) return;
    String mac = WiFi.macAddress(); mac.replace(":", "");
    String devId = "frankenchan_" + mac;
    String dev = "\"dev\":{\"ids\":[\"" + devId + "\"],\"name\":\"" + cfg.name +
                 "\",\"mdl\":\"StackChan\",\"mf\":\"FrankenChan\"}";

    struct Sensor { const char* id; const char* name; const char* tmpl; const char* unit; const char* dc; };
    static const Sensor sensors[] = {
        {"mood",     "Mood",            "{{ value_json.mood }}",        "",     ""},
        {"battery",  "Battery",         "{{ value_json.battery_pct }}", "%",    "battery"},
        {"rssi",     "WiFi signal",     "{{ value_json.rssi }}",        "dBm",  "signal_strength"},
        {"latency",  "Internet latency","{{ value_json.latency_ms }}",  "ms",   ""},
        {"presence", "Desk presence",   "{{ value_json.near }}",        "",     ""},
    };

    for (auto& s : sensors) {
        String cfgTopic = "homeassistant/sensor/" + devId + "_" + s.id + "/config";
        String payload = String("{\"name\":\"") + s.name + "\",\"uniq_id\":\"" + devId + "_" + s.id +
                         "\",\"stat_t\":\"" + topic("state") + "\",\"val_tpl\":\"" + s.tmpl + "\"";
        if (strlen(s.unit)) payload += String(",\"unit_of_meas\":\"") + s.unit + "\"";
        if (strlen(s.dc))   payload += String(",\"dev_cla\":\"") + s.dc + "\"";
        payload += "," + dev + "}";
        mq.publish(cfgTopic.c_str(), payload.c_str(), true);
    }

    // a text entity so HA can make it talk
    String sayTopic = "homeassistant/text/" + devId + "_say/config";
    String sayPayload = "{\"name\":\"Say\",\"uniq_id\":\"" + devId + "_say\",\"cmd_t\":\"" +
                        topic("say") + "\",\"stat_t\":\"" + topic("last_said") + "\"," + dev + "}";
    mq.publish(sayTopic.c_str(), sayPayload.c_str(), true);
}

/* -------------------------------- callback ------------------------------ */

static void onMessage(char* t, byte* payload, unsigned int len) {
    String topicStr = String(t);
    String msg;
    msg.reserve(len + 1);
    for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
    msg.trim();
    if (!msg.length()) return;

    if (topicStr.endsWith("/cmd")) {
        runAction(msg);
    } else if (topicStr.endsWith("/say")) {
        runAction("say:" + msg);
        mq.publish(topic("last_said").c_str(), msg.c_str(), true);
    } else if (topicStr.endsWith("/mood")) {
        runAction("mood:" + msg);
    }
}

/* --------------------------------- state -------------------------------- */

static void publishState() {
    float v = M5StackChan.getBatteryVoltage();
    int pct = (int)constrain((v - 3.3f) / (4.2f - 3.3f) * 100.0f, 0.0f, 100.0f);
    String j = String("{\"mood\":\"") + moodName(moodCurrent()) + "\"" +
               ",\"battery_pct\":" + String(pct) +
               ",\"battery_v\":" + String(v, 2) +
               ",\"rssi\":" + String(netRssi()) +
               ",\"internet_up\":" + String(netInternetUp() ? "true" : "false") +
               ",\"latency_ms\":" + String(netLatencyMs()) +
               ",\"near\":" + String(senseNear() ? "true" : "false") +
               ",\"uptime_s\":" + String(millis() / 1000) + "}";
    mq.publish(topic("state").c_str(), j.c_str(), true);
}

/* --------------------------------- public ------------------------------- */

void mqttInit() {
    if (!cfg.mqtt_host.length()) return;
    baseTopic = cfg.mqtt_topic.length() ? cfg.mqtt_topic : String("frankenchan");
    mq.setServer(cfg.mqtt_host.c_str(), cfg.mqtt_port ? cfg.mqtt_port : 1883);
    mq.setCallback(onMessage);
    mq.setBufferSize(768);   // HA discovery payloads are chunky
}

bool mqttConnected() { return mq.connected(); }

void mqttUpdate() {
    if (!cfg.mqtt_host.length() || WiFi.status() != WL_CONNECTED) return;

    if (!mq.connected()) {
        uint32_t now = millis();
        if (now < nextRetry) return;
        nextRetry = now + 5000;

        String clientId = "frankenchan-" + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);
        String willTopic = topic("available");
        bool ok = cfg.mqtt_user.length()
            ? mq.connect(clientId.c_str(), cfg.mqtt_user.c_str(), cfg.mqtt_pass.c_str(),
                         willTopic.c_str(), 0, true, "offline")
            : mq.connect(clientId.c_str(), willTopic.c_str(), 0, true, "offline");
        if (!ok) return;

        mq.publish(willTopic.c_str(), "online", true);
        mq.subscribe(topic("cmd").c_str());
        mq.subscribe(topic("say").c_str());
        mq.subscribe(topic("mood").c_str());
        publishDiscovery();
        publishState();
        return;
    }

    mq.loop();

    uint32_t now = millis();
    if (now >= nextState) {
        nextState = now + 30000;
        publishState();
    }
}

void mqttPublishEvent(const String& kind, const String& detail) {
    if (!mq.connected()) return;
    String j = "{\"kind\":\"" + kind + "\",\"detail\":\"" + detail + "\"}";
    mq.publish(topic("event").c_str(), j.c_str(), false);
}
