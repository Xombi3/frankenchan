#include "fc_action.h"
#include "fc_mood.h"
#include "fc_motion.h"
#include "fc_face.h"
#include "fc_apps.h"
#include "fc_ir.h"
#include "fc_voice.h"
#include "fc_netmon.h"

static bool startsWith(const String& s, const char* p, String& rest) {
    String pre = String(p);
    if (!s.startsWith(pre)) return false;
    rest = s.substring(pre.length());
    rest.trim();
    return true;
}

bool runAction(const String& raw) {
    String a = raw;
    a.trim();
    if (!a.length()) return false;

    String arg;

    if (a.equalsIgnoreCase("chat"))   { voiceChatRound(); return true; }
    if (a.equalsIgnoreCase("wink"))   { faceWink(); return true; }
    if (a.equalsIgnoreCase("nod"))    { gestureNod(); return true; }
    if (a.equalsIgnoreCase("shake"))  { gestureShake(); return true; }
    if (a.equalsIgnoreCase("tilt"))   { gestureTilt(); return true; }
    if (a.equalsIgnoreCase("wiggle")) { gestureWiggle(); return true; }
    if (a.equalsIgnoreCase("scan"))   { netRequestScan(); return true; }

    if (startsWith(a, "say:", arg))   { speakText(arg); return true; }
    if (startsWith(a, "ir:", arg))    { return irSend(arg); }
    if (startsWith(a, "face:", arg))  { return faceSetStyle(arg); }
    if (startsWith(a, "theme:", arg)) { return faceSetTheme(arg); }

    if (startsWith(a, "mood:", arg)) {
        Mood m;
        if (!moodFromName(arg, m)) return false;
        moodSet(m, 8000); moodChirp(m);
        return true;
    }

    if (startsWith(a, "timer:", arg)) {
        if (arg.equalsIgnoreCase("cancel")) { appsTimerCancel(); return true; }
        long mins = arg.toInt();
        if (mins <= 0) return false;
        appsTimerAdd(mins * 60);
        return true;
    }

    if (startsWith(a, "mode:", arg)) {
        const char* names[] = {"face", "clock", "weather", "timer", "network"};
        for (int i = 0; i < 5; i++) {
            if (arg.equalsIgnoreCase(names[i])) { appsSetMode((FCMode)i); return true; }
        }
        return false;
    }

    return false;
}
