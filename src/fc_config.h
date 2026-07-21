// FrankenChan configuration (NVS-backed)
#pragma once
#include <Arduino.h>

struct FCConfig {
    String wifi_ssid, wifi_pass;
    String name        = "FrankenChan";
    String tz          = "UTC0";
    String city        = "";
    String ping_host    = "1.1.1.1";
    String devices      = "";  // "Name:IP,Name:IP"
    // face customization
    String face_style   = "classic";  // classic|doggy|omega|girly|demon|cyclops
    String face_theme   = "classic";  // mint|sunset|mono|demon|sky|classic
    String eye_hex      = "";          // optional RRGGBB overrides theme
    String bg_hex       = "";
    String cheek_hex    = "";
    uint8_t show_batt   = 1;
    uint8_t brightness  = 80;
    // NFC tap routines: "UID:action,UID:action"
    String nfc_tags     = "";
    // IMU gesture actions (see fc_action.h); blank = built-in default reaction
    String gesture_pickup = "";
    String gesture_shake  = "";
    String gesture_tap    = "";
    // presence sensing
    uint16_t prox_threshold   = 300;   // LTR-553 PS raw value
    uint8_t  prox_greet       = 1;     // perk up when someone approaches
    uint16_t dim_after_s      = 300;   // 0 = never dim
    uint8_t  camera_on        = 0;     // opt-in: shares I2C during init
    uint8_t  motion_sensitivity = 12;  // per-cell grayscale delta
    // ESP-NOW
    uint8_t espnow_speak = 1;          // read alert text out loud
    // MQTT
    String   mqtt_host  = "";
    uint16_t mqtt_port  = 1883;
    String   mqtt_user  = "";
    String   mqtt_pass  = "";
    String   mqtt_topic = "frankenchan";
    uint8_t  mqtt_ha_discovery = 1;
    String api_base    = "https://api.openai.com/v1";
    String api_key     = "";
    String chat_model  = "gpt-4o-mini";
    String stt_model   = "whisper-1";
    String tts_model   = "gpt-4o-mini-tts";
    String tts_voice   = "alloy";
    String personality = "You are a tiny cheerful desktop robot who lives on a desk. "
                         "You are curious, a bit dramatic, and love your human.";
    float lat = 0, lon = 0;
    uint8_t volume = 160;
    void load();
    void save();
};
extern FCConfig cfg;
