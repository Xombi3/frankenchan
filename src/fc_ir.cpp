#include "fc_ir.h"
#include "fc_mood.h"
#include <Preferences.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>

static const uint16_t IR_TX_PIN = 5;
static const uint16_t IR_RX_PIN = 10;
static const uint16_t CAPTURE_BUF = 1024;
static const uint8_t  CAPTURE_TIMEOUT = 15;

static IRsend irsend(IR_TX_PIN);
static IRrecv irrecv(IR_RX_PIN, CAPTURE_BUF, CAPTURE_TIMEOUT, true);
static decode_results results;

static std::vector<IRCommand> cmds;
static String learnName = "";
static uint32_t learnUntil = 0;

static Preferences irPrefs;

/* ------------------------------ persistence ----------------------------- */
// stored as one packed string: name|proto|code|bits;name|proto|code|bits

static void saveAll() {
    String blob;
    for (auto& c : cmds) {
        blob += c.name + "|" + c.proto + "|" + String((unsigned long long)c.code) +
                "|" + String(c.bits) + ";";
    }
    irPrefs.begin("fc_ir", false);
    irPrefs.putString("cmds", blob);
    irPrefs.end();
}

static void loadAll() {
    irPrefs.begin("fc_ir", true);
    String blob = irPrefs.getString("cmds", "");
    irPrefs.end();
    cmds.clear();
    int start = 0;
    while (start < (int)blob.length()) {
        int semi = blob.indexOf(';', start);
        if (semi < 0) break;
        String rec = blob.substring(start, semi);
        start = semi + 1;
        int p1 = rec.indexOf('|'), p2 = rec.indexOf('|', p1 + 1), p3 = rec.indexOf('|', p2 + 1);
        if (p1 < 0 || p2 < 0 || p3 < 0) continue;
        IRCommand c;
        c.name  = rec.substring(0, p1);
        c.proto = rec.substring(p1 + 1, p2);
        c.code  = strtoull(rec.substring(p2 + 1, p3).c_str(), nullptr, 10);
        c.bits  = rec.substring(p3 + 1).toInt();
        cmds.push_back(c);
    }
}

/* --------------------------------- init --------------------------------- */

void irInit() {
    irsend.begin();
    irrecv.enableIRIn();
    loadAll();
}

/* ------------------------------- learning ------------------------------- */

bool irLearnStart(const String& name) {
    if (!name.length()) return false;
    learnName = name;
    learnUntil = millis() + 15000;   // 15s window to press a button
    irrecv.resume();
    return true;
}

bool irLearning() { return learnName.length() > 0; }
void irLearnCancel() { learnName = ""; }

void irUpdate() {
    if (!learnName.length()) return;

    if (millis() > learnUntil) {           // timed out
        learnName = "";
        moodSet(Mood::Doubt, 2500);
        moodChirp(Mood::Doubt);
        return;
    }

    if (irrecv.decode(&results)) {
        if (results.decode_type != decode_type_t::UNKNOWN && results.bits > 0) {
            IRCommand c;
            c.name  = learnName;
            c.proto = typeToString(results.decode_type, false);
            c.code  = results.value;
            c.bits  = results.bits;
            // replace existing entry with the same name
            for (auto it = cmds.begin(); it != cmds.end(); ++it) {
                if (it->name.equalsIgnoreCase(c.name)) { cmds.erase(it); break; }
            }
            if (cmds.size() < 32) cmds.push_back(c);
            saveAll();
            learnName = "";
            moodSet(Mood::Happy, 3000);
            moodChirp(Mood::Happy);
        }
        irrecv.resume();
    }
}

/* -------------------------------- sending ------------------------------- */

bool irSend(const String& name) {
    for (auto& c : cmds) {
        if (!c.name.equalsIgnoreCase(name)) continue;
        decode_type_t t = strToDecodeType(c.proto.c_str());
        if (t == decode_type_t::UNKNOWN) return false;
        // hasACState protocols need a byte array we don't store; skip those
        if (hasACState(t)) return false;
        bool ok = irsend.send(t, c.code, c.bits);
        if (ok) { moodSet(Mood::Happy, 1500); }
        irrecv.resume();   // sending disturbs the receiver
        return ok;
    }
    return false;
}

/* --------------------------------- misc --------------------------------- */

std::vector<IRCommand> irList() { return cmds; }

bool irForget(const String& name) {
    for (auto it = cmds.begin(); it != cmds.end(); ++it) {
        if (it->name.equalsIgnoreCase(name)) {
            cmds.erase(it);
            saveAll();
            return true;
        }
    }
    return false;
}

String irJson() {
    String j = "{\"learning\":" + String(irLearning() ? "true" : "false") +
               ",\"learn_name\":\"" + learnName + "\",\"commands\":[";
    for (size_t i = 0; i < cmds.size(); i++) {
        if (i) j += ",";
        j += "{\"name\":\"" + cmds[i].name + "\",\"proto\":\"" + cmds[i].proto +
             "\",\"bits\":" + String(cmds[i].bits) + "}";
    }
    j += "]}";
    return j;
}
