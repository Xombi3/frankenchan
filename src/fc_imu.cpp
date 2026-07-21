#include "fc_imu.h"
#include "fc_config.h"
#include "fc_action.h"
#include "fc_mood.h"
#include "fc_motion.h"
#include "fc_mqtt.h"
#include <M5Unified.h>

static bool held = false;
static bool movedFlag = false;
static float lastMag = 1.0f;
static uint32_t shakeCount = 0;
static uint32_t shakeWindowStart = 0;
static uint32_t lastGestureAt = 0;
static float peakMag = 0;

void imuInit() {
    // M5Unified brings the IMU up in M5.begin(); nothing else needed.
}

void imuUpdate() {
    static uint32_t next = 0;
    uint32_t now = millis();
    if (now < next) return;
    next = now + 60;

    if (!M5.Imu.update()) return;
    auto d = M5.Imu.getImuData();
    float ax = d.accel.x, ay = d.accel.y, az = d.accel.z;
    float mag = sqrtf(ax * ax + ay * ay + az * az);   // ~1.0g at rest
    float delta = fabsf(mag - lastMag);
    lastMag = mag;
    if (delta > peakMag) peakMag = delta;

    bool cooling = (now - lastGestureAt) < 1500;

    // --- shake: several sharp jolts inside a short window ---
    if (delta > 0.45f) {
        if (now - shakeWindowStart > 1200) { shakeWindowStart = now; shakeCount = 0; }
        shakeCount++;
        movedFlag = true;
        if (shakeCount >= 4 && !cooling) {
            shakeCount = 0;
            lastGestureAt = now;
            mqttPublishEvent("gesture", "shake");
            if (cfg.gesture_shake.length()) runAction(cfg.gesture_shake);
            else { moodSet(Mood::Doubt, 2500); moodChirp(Mood::Doubt); }
            return;
        }
    }

    // --- pick up / put down: sustained tilt away from flat ---
    // az near 1.0 when sitting upright on a desk; a big change means it was lifted
    bool nowHeld = (fabsf(az) < 0.72f) || (mag > 1.25f);
    if (nowHeld != held) {
        static uint32_t stableSince = 0;
        if (!stableSince) stableSince = now;
        if (now - stableSince > 400) {         // debounce
            held = nowHeld;
            stableSince = 0;
            movedFlag = true;
            if (!cooling) {
                lastGestureAt = now;
                if (held) {
                    motionIdleEnable(false);   // don't flail while in a hand
                    mqttPublishEvent("gesture", "pickup");
                    if (cfg.gesture_pickup.length()) runAction(cfg.gesture_pickup);
                    else { moodSet(Mood::Excited, 3000); moodChirp(Mood::Excited); }
                } else {
                    motionIdleEnable(true);
                    moodSet(Mood::Happy, 2000);
                }
            }
        }
    }

    // --- desk tap: single sharp spike while still flat on the desk ---
    if (!held && delta > 0.30f && delta < 0.45f && !cooling) {
        lastGestureAt = now;
        mqttPublishEvent("gesture", "desk_tap");
        if (cfg.gesture_tap.length()) runAction(cfg.gesture_tap);
    }
}

bool imuHeldUp() { return held; }

bool imuWasMoved() {
    bool v = movedFlag;
    movedFlag = false;
    return v;
}

String imuJson() {
    return String("{\"held\":") + (held ? "true" : "false") +
           ",\"peak_delta_g\":" + String(peakMag, 2) + "}";
}
