/*
 * FrankenChan — custom firmware for the official M5Stack StackChan (K151 / CoreS3)
 *
 * A complete desktop-robot package:
 *  - Living face with moods, blinking, idle quirks (m5stack-avatar)
 *  - Expressive head motion + gesture library (nod / shake / tilt / wiggle)
 *  - AI voice chat: tap its back -> talk -> it answers out loud (OpenAI-compatible APIs)
 *  - Glance modes on body swipe: Face / Clock / Weather / Timer
 *  - Web setup portal + REST API (IoT / Home Assistant friendly)
 *  - Mood RGB lighting, battery-aware sleepy face, cheek-poke reactions
 *  - Network monitor: signal, internet health, AP scan, device presence
 *  - Universal IR remote (learn + replay your existing remotes)
 *  - NFC tap routines, IMU gestures (pick up / shake / desk tap)
 *  - Camera motion presence + proximity wake, auto-dim when away
 *  - ESP-NOW sensor mesh (it turns to look at whichever sensor fired)
 *  - MQTT bridge with Home Assistant auto-discovery
 *
 * Controls
 *  - Tap back touch panel .... start/stop voice chat
 *  - Swipe back panel ........ switch glance mode
 *  - Poke a cheek (screen) ... giggle
 *  - Tap forehead (screen) ... in Timer mode +1 min, hold to cancel
 *
 * First boot: it opens Wi-Fi network "FrankenChan-xxxx" (password: stackchan).
 * Connect and the setup page pops up. Later: http://frankenchan.local
 */
#include <Arduino.h>
#include <M5StackChan.h>
#include <M5Unified.h>
#include <Avatar.h>
#include <WiFi.h>
#include "fc_config.h"
#include "fc_mood.h"
#include "fc_motion.h"
#include "fc_voice.h"
#include "fc_apps.h"
#include "fc_web.h"
#include "fc_netmon.h"
#include "fc_face.h"
#include "fc_action.h"
#include "fc_ir.h"
#include "fc_nfc.h"
#include "fc_imu.h"
#include "fc_sense.h"
#include "fc_espnow.h"
#include "fc_mqtt.h"

using namespace m5avatar;

static Avatar avatar;

static bool wifiConnect() {
    if (!cfg.wifi_ssid.length()) return false;
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("frankenchan");
    WiFi.begin(cfg.wifi_ssid.c_str(), cfg.wifi_pass.c_str());
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(200);
    return WiFi.status() == WL_CONNECTED;
}

void setup() {
    cfg.load();
    M5StackChan.begin();
    M5.Speaker.begin();
    M5.Speaker.setVolume(cfg.volume);

    avatar.init(8);          // 8-bit color mode for custom themes
    faceInit(&avatar);       // apply saved face style + colors
    moodInit(&avatar);
    voiceInit(&avatar);
    appsInit(&avatar);
    motionInit();
    irInit();
    nfcInit();
    imuInit();
    senseInit();
    M5.Display.setBrightness(cfg.brightness);

    // boot jingle + wave
    moodSet(Mood::Excited, 3000);
    M5.Speaker.tone(1000, 80); delay(100);
    M5.Speaker.tone(1500, 80); delay(100);
    M5.Speaker.tone(2000, 150);

    bool online = wifiConnect();
    if (online) {
        configTzTime(cfg.tz.c_str(), "pool.ntp.org", "time.nist.gov");
        webStart(false);
        netmonInit();
        espnowInit();
        mqttInit();
        avatar.setSpeechText("Hi! I'm alive!");
        gestureWiggle();
    } else {
        webStart(true);  // captive setup portal
        avatar.setSpeechText("Join FrankenChan-xxxx WiFi!");
        moodSet(Mood::Doubt, 60000);
    }
    delay(1500);
    avatar.setSpeechText("");
}

static void handleScreenTouch() {
    auto t = M5.Touch.getDetail();
    if (!t.wasPressed()) return;

    int w = M5.Display.width(), h = M5.Display.height();

    if (appsMode() == FCMode::Timer && t.y < h / 2) {
        // forehead in timer mode: tap +1 min (hold handled below)
        appsTimerAdd(60);
        return;
    }
    if (t.x < w / 3) {          // left cheek
        moodSet(Mood::Happy, 3000); moodChirp(Mood::Happy); gestureTilt();
    } else if (t.x > 2 * w / 3) {  // right cheek
        moodSet(Mood::Excited, 3000); moodChirp(Mood::Excited); faceWink(); gestureNod();
    } else {
        moodSet(Mood::Doubt, 2000);
    }
}

static void handleScreenHold() {
    auto t = M5.Touch.getDetail();
    if (t.wasHold() && appsMode() == FCMode::Timer) appsTimerCancel();
}

void loop() {
    M5StackChan.update();

    auto& ts = M5StackChan.TouchSensor;
    if (ts.wasClicked())            voiceChatRound();
    if (ts.wasSwipedForward())      appsNextMode(+1);
    if (ts.wasSwipedBackward())     appsNextMode(-1);

    handleScreenTouch();
    handleScreenHold();
    moodUpdate();
    appsUpdate();
    netmonUpdate();
    faceUpdate();
    irUpdate();
    nfcUpdate();
    imuUpdate();
    senseUpdate();
    espnowUpdate();
    mqttUpdate();
    webUpdate();

    delay(20);
}
