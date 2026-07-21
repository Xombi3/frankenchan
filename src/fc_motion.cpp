#include "fc_motion.h"
#include <M5StackChan.h>

static volatile bool idleOn = true;
static volatile bool busy = false;

// All motion goes through here so nothing can command an end stop.
void motionSafeMove(int yaw, int pitch, int speed) {
    yaw   = constrain(yaw,   FC_YAW_MIN,   FC_YAW_MAX);
    pitch = constrain(pitch, FC_PITCH_MIN, FC_PITCH_MAX);
    M5StackChan.Motion.move(yaw, pitch, speed);
}

static void safePitch(int pitch, int speed) {
    M5StackChan.Motion.moveY(constrain(pitch, FC_PITCH_MIN, FC_PITCH_MAX), speed);
}

static void safeYaw(int yaw, int speed) {
    M5StackChan.Motion.moveX(constrain(yaw, FC_YAW_MIN, FC_YAW_MAX), speed);
}

static void idleTask(void*) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000 + (esp_random() % 6000)));
        if (!idleOn || busy) continue;
        // small random glance: yaw +-20 deg, pitch 10..45 deg
        int yaw   = (int)(esp_random() % 400) - 200;
        int pitch = 100 + (int)(esp_random() % 350);
        motionSafeMove(yaw, pitch, 200);
        vTaskDelay(pdMS_TO_TICKS(1800 + (esp_random() % 1500)));
        if (!idleOn || busy) continue;
        if ((esp_random() % 100) < 60) motionSafeMove(0, 250, 150);
    }
}

void motionInit() {
    // Match the factory firmware: let the servos de-energise when they are not
    // moving. Without this they hold torque forever - they run hot, buzz, and
    // chew through the battery for no reason.
    M5StackChan.Motion.setAutoTorqueReleaseEnabled(true);

    M5StackChan.Motion.goHome(300);
    delay(300);
    motionSafeMove(0, 250, 200);  // slightly raised, friendly posture
    xTaskCreatePinnedToCore(idleTask, "fc_idle", 3072, nullptr, 1, nullptr, 0);
}

void motionIdleEnable(bool en) { idleOn = en; }

void gestureNod() {
    busy = true;
    for (int i = 0; i < 2; i++) {
        safePitch(450, 900); delay(260);
        safePitch(100, 900); delay(260);
    }
    safePitch(250, 400);
    busy = false;
}

void gestureShake() {
    busy = true;
    for (int i = 0; i < 2; i++) {
        safeYaw(250, 900);  delay(240);
        safeYaw(-250, 900); delay(240);
    }
    safeYaw(0, 500);
    busy = false;
}

void gestureTilt() {
    busy = true;
    motionSafeMove(300, 400, 500); delay(700);
    motionSafeMove(0, 250, 300);
    busy = false;
}

void gestureWiggle() {
    busy = true;
    for (int i = 0; i < 3; i++) {
        motionSafeMove(200, 500, 1000);  delay(200);
        motionSafeMove(-200, 150, 1000); delay(200);
    }
    motionSafeMove(0, 250, 400);
    busy = false;
}

void gestureLook(float nx, float ny) {
    busy = true;
    // lookAtNormalized spreads -1..1 over the servos' full mechanical range, so
    // the extremes land exactly on the end stops. Hold back to the safe band.
    nx = constrain(nx, -0.95f, 0.95f);
    ny = constrain(ny, -0.90f, 0.90f);   // ~pitch 45..855, inside 30..870
    M5StackChan.Motion.lookAtNormalized(nx, ny, 600);
    delay(500);
    busy = false;
}

void gestureLookBearing(int degrees) {
    busy = true;
    motionSafeMove(constrain(degrees, -120, 120) * 10, 300, 600);
    busy = false;
}
