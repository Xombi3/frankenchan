#include "fc_config.h"
#include <Preferences.h>

FCConfig cfg;
static Preferences prefs;

void FCConfig::load() {
    prefs.begin("frankenchan", true);
    wifi_ssid   = prefs.getString("ssid", wifi_ssid);
    wifi_pass   = prefs.getString("pass", wifi_pass);
    name        = prefs.getString("name", name);
    tz          = prefs.getString("tz", tz);
    city        = prefs.getString("city", city);
    ping_host   = prefs.getString("ping", ping_host);
    devices     = prefs.getString("devs", devices);
    face_style  = prefs.getString("fstyle", face_style);
    face_theme  = prefs.getString("ftheme", face_theme);
    eye_hex     = prefs.getString("eye", eye_hex);
    bg_hex      = prefs.getString("bg", bg_hex);
    cheek_hex   = prefs.getString("cheek", cheek_hex);
    show_batt   = prefs.getUChar("sbatt", show_batt);
    brightness  = prefs.getUChar("bright", brightness);
    nfc_tags    = prefs.getString("nfc", nfc_tags);
    gesture_pickup = prefs.getString("gpick", gesture_pickup);
    gesture_shake  = prefs.getString("gshake", gesture_shake);
    gesture_tap    = prefs.getString("gtap", gesture_tap);
    prox_threshold = prefs.getUShort("prox", prox_threshold);
    prox_greet     = prefs.getUChar("pgreet", prox_greet);
    dim_after_s    = prefs.getUShort("dim", dim_after_s);
    camera_on      = prefs.getUChar("cam", camera_on);
    motion_sensitivity = prefs.getUChar("msens", motion_sensitivity);
    espnow_speak   = prefs.getUChar("espk", espnow_speak);
    mqtt_host   = prefs.getString("mqhost", mqtt_host);
    mqtt_port   = prefs.getUShort("mqport", mqtt_port);
    mqtt_user   = prefs.getString("mquser", mqtt_user);
    mqtt_pass   = prefs.getString("mqpass", mqtt_pass);
    mqtt_topic  = prefs.getString("mqtop", mqtt_topic);
    mqtt_ha_discovery = prefs.getUChar("mqha", mqtt_ha_discovery);
    api_base    = prefs.getString("api_base", api_base);
    api_key     = prefs.getString("api_key", api_key);
    chat_model  = prefs.getString("chat_m", chat_model);
    stt_model   = prefs.getString("stt_m", stt_model);
    tts_model   = prefs.getString("tts_m", tts_model);
    tts_voice   = prefs.getString("tts_v", tts_voice);
    personality = prefs.getString("pers", personality);
    lat         = prefs.getFloat("lat", 0);
    lon         = prefs.getFloat("lon", 0);
    volume      = prefs.getUChar("vol", volume);
    prefs.end();
}

void FCConfig::save() {
    prefs.begin("frankenchan", false);
    prefs.putString("ssid", wifi_ssid);
    prefs.putString("pass", wifi_pass);
    prefs.putString("name", name);
    prefs.putString("tz", tz);
    prefs.putString("city", city);
    prefs.putString("ping", ping_host);
    prefs.putString("devs", devices);
    prefs.putString("fstyle", face_style);
    prefs.putString("ftheme", face_theme);
    prefs.putString("eye", eye_hex);
    prefs.putString("bg", bg_hex);
    prefs.putString("cheek", cheek_hex);
    prefs.putUChar("sbatt", show_batt);
    prefs.putUChar("bright", brightness);
    prefs.putString("nfc", nfc_tags);
    prefs.putString("gpick", gesture_pickup);
    prefs.putString("gshake", gesture_shake);
    prefs.putString("gtap", gesture_tap);
    prefs.putUShort("prox", prox_threshold);
    prefs.putUChar("pgreet", prox_greet);
    prefs.putUShort("dim", dim_after_s);
    prefs.putUChar("cam", camera_on);
    prefs.putUChar("msens", motion_sensitivity);
    prefs.putUChar("espk", espnow_speak);
    prefs.putString("mqhost", mqtt_host);
    prefs.putUShort("mqport", mqtt_port);
    prefs.putString("mquser", mqtt_user);
    prefs.putString("mqpass", mqtt_pass);
    prefs.putString("mqtop", mqtt_topic);
    prefs.putUChar("mqha", mqtt_ha_discovery);
    prefs.putString("api_base", api_base);
    prefs.putString("api_key", api_key);
    prefs.putString("chat_m", chat_model);
    prefs.putString("stt_m", stt_model);
    prefs.putString("tts_m", tts_model);
    prefs.putString("tts_v", tts_voice);
    prefs.putString("pers", personality);
    prefs.putFloat("lat", lat);
    prefs.putFloat("lon", lon);
    prefs.putUChar("vol", volume);
    prefs.end();
}
