#include "fc_nfc.h"
#include "fc_config.h"
#include "fc_action.h"
#include "fc_mood.h"
#include "fc_mqtt.h"
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>
#include <vector>

using namespace m5::nfc::a;

namespace {
m5::unit::UnitUnified Units;
m5::unit::UnitNFC unit{};
m5::nfc::NFCLayerA nfc_a{unit};
bool ready = false;
String lastUid = "";
uint32_t lastSeen = 0;
String lastActedUid = "";
uint32_t lastActedAt = 0;
}

void nfcInit() {
    // The NFC unit lives on the internal I2C bus; failure here is not fatal.
    if (Units.add(unit, M5.In_I2C) && Units.begin()) {
        ready = true;
    }
}

bool nfcAvailable() { return ready; }
String nfcLastUid() { return lastUid; }
uint32_t nfcLastSeenMs() { return lastSeen; }

// cfg.nfc_tags format: "UID:action,UID:action"  e.g. "04A2B3C4:timer:25,04FF11:chat"
static String actionForUid(const String& uid) {
    String s = cfg.nfc_tags;
    int start = 0;
    while (start < (int)s.length()) {
        int comma = s.indexOf(',', start);
        String tok = (comma < 0) ? s.substring(start) : s.substring(start, comma);
        tok.trim();
        int colon = tok.indexOf(':');
        if (colon > 0) {
            String u = tok.substring(0, colon); u.trim();
            String act = tok.substring(colon + 1); act.trim();
            if (u.equalsIgnoreCase(uid)) return act;
        }
        if (comma < 0) break;
        start = comma + 1;
    }
    return "";
}

void nfcUpdate() {
    if (!ready) return;

    static uint32_t next = 0;
    uint32_t now = millis();
    if (now < next) return;
    next = now + 400;          // poll a few times a second

    Units.update();

    std::vector<PICC> piccs;
    if (!nfc_a.detect(piccs)) return;

    for (auto&& u : piccs) {
        String uid = String(u.uidAsString().c_str());
        if (!uid.length()) continue;
        lastUid = uid;
        lastSeen = now;

        // debounce: same tag can't retrigger within 3s
        if (uid.equalsIgnoreCase(lastActedUid) && now - lastActedAt < 3000) continue;

        String act = actionForUid(uid);
        M5.Speaker.tone(2200, 40);
        if (act.length()) {
            lastActedUid = uid;
            lastActedAt = now;
            mqttPublishEvent("nfc", uid + " -> " + act);
            if (!runAction(act)) { moodSet(Mood::Doubt, 2000); }
        } else {
            // unknown tag: acknowledge so the user can copy the UID from the UI
            mqttPublishEvent("nfc_unknown", uid);
            moodSet(Mood::Doubt, 2500);
        }
        break;
    }
    nfc_a.deactivate();
}

String nfcJson() {
    return String("{\"available\":") + (ready ? "true" : "false") +
           ",\"last_uid\":\"" + lastUid + "\"" +
           ",\"last_seen_ms_ago\":" + String(lastSeen ? (millis() - lastSeen) : 0) + "}";
}
