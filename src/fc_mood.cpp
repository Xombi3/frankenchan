#include "fc_mood.h"
#include <M5StackChan.h>
#include <M5Unified.h>

using namespace m5avatar;

static Avatar* avatar = nullptr;
static Mood cur = Mood::Neutral;
static uint32_t holdUntil = 0;
static uint32_t nextIdleQuirk = 0;

struct MoodLook { Expression exp; uint8_t r, g, b; };

static MoodLook look(Mood m) {
    switch (m) {
        case Mood::Happy:   return {Expression::Happy,  255, 170,  40};
        case Mood::Excited: return {Expression::Happy,  255,  60, 180};
        case Mood::Sleepy:  return {Expression::Sleepy,  40,  40, 120};
        case Mood::Doubt:   return {Expression::Doubt,  120, 255, 120};
        case Mood::Sad:     return {Expression::Sad,     40, 120, 255};
        case Mood::Angry:   return {Expression::Angry,  255,  40,  40};
        default:            return {Expression::Neutral, 90, 200, 255};
    }
}

const char* moodName(Mood m) {
    switch (m) {
        case Mood::Happy: return "happy";     case Mood::Excited: return "excited";
        case Mood::Sleepy: return "sleepy";   case Mood::Doubt: return "doubt";
        case Mood::Sad: return "sad";         case Mood::Angry: return "angry";
        default: return "neutral";
    }
}

bool moodFromName(const String& s, Mood& out) {
    for (Mood m : {Mood::Neutral, Mood::Happy, Mood::Excited, Mood::Sleepy,
                   Mood::Doubt, Mood::Sad, Mood::Angry}) {
        if (s.equalsIgnoreCase(moodName(m))) { out = m; return true; }
    }
    return false;
}

static void apply(Mood m) {
    cur = m;
    MoodLook lk = look(m);
    if (avatar) avatar->setExpression(lk.exp);
    // soft glow at 1/4 brightness so it is not blinding
    M5StackChan.showRgbColor(lk.r / 4, lk.g / 4, lk.b / 4);
}

void moodInit(Avatar* av) {
    avatar = av;
    apply(Mood::Neutral);
    nextIdleQuirk = millis() + 15000;
}

void moodSet(Mood m, uint32_t holdMs) {
    apply(m);
    holdUntil = millis() + holdMs;
}

Mood moodCurrent() { return cur; }

void moodChirp(Mood m) {
    auto& spk = M5.Speaker;
    switch (m) {
        case Mood::Happy:
        case Mood::Excited:
            spk.tone(1200, 80); delay(90); spk.tone(1800, 80); delay(90); spk.tone(2400, 120);
            break;
        case Mood::Sad:
            spk.tone(900, 150); delay(160); spk.tone(600, 250);
            break;
        case Mood::Angry:
            spk.tone(300, 120); delay(130); spk.tone(300, 120);
            break;
        case Mood::Doubt:
            spk.tone(800, 100); delay(110); spk.tone(1100, 180);
            break;
        case Mood::Sleepy:
            spk.tone(500, 300); delay(320); spk.tone(400, 400);
            break;
        default:
            spk.tone(1000, 90);
    }
}

void moodUpdate() {
    uint32_t now = millis();

    // decay temporary moods back to neutral
    if (cur != Mood::Neutral && holdUntil && now > holdUntil) {
        holdUntil = 0;
        apply(Mood::Neutral);
    }

    // low battery -> sleepy face
    static uint32_t nextBatt = 0;
    if (now > nextBatt) {
        nextBatt = now + 30000;
        float v = M5StackChan.getBatteryVoltage();
        if (v > 2.0f && v < 3.45f && cur == Mood::Neutral) {
            moodSet(Mood::Sleepy, 20000);
        }
    }

    // random idle quirks so the face feels alive
    if (cur == Mood::Neutral && now > nextIdleQuirk) {
        nextIdleQuirk = now + 20000 + (esp_random() % 40000);
        int r = esp_random() % 100;
        if (r < 45)      moodSet(Mood::Happy, 2500);
        else if (r < 60) moodSet(Mood::Doubt, 2000);
        else if (r < 70) moodSet(Mood::Sleepy, 3000);
        // otherwise stay neutral this round
    }
}
