#include "fc_face.h"
#include "fc_config.h"
#include <M5Unified.h>
#include <M5StackChan.h>
#include <faces/FaceTemplates.hpp>

using namespace m5avatar;

static Avatar* avatar = nullptr;

// A custom face: round eyes + gentle U mouth + bow brows (from the library demo)
class CyclopsFace : public Face {
public:
    CyclopsFace()
        : Face(new UShapeMouth(44, 44, 0, 16), new BoundingRect(222, 160),
               new EllipseEye(34, 34, false), new BoundingRect(163, 92),
               new EllipseEye(34, 34, true),  new BoundingRect(163, 228),
               new BowEyebrow(64, 20, false), new BoundingRect(150, 92),
               new BowEyebrow(64, 20, true),  new BoundingRect(150, 228)) {}
};

static Face* classicFace = nullptr;   // native face captured from avatar

/* --------------------------- color utilities ---------------------------- */

static bool parseHex(const String& h, uint32_t& out) {
    String s = h;
    if (s.startsWith("#")) s = s.substring(1);
    if (s.length() != 6) return false;
    char* end = nullptr;
    long v = strtol(s.c_str(), &end, 16);
    if (end == s.c_str() || *end) return false;
    out = (uint32_t)v;
    return true;
}

static uint16_t to565(uint32_t rgb) { return M5.Display.color24to16(rgb); }

/* ------------------------------ apply look ------------------------------ */

static Face* faceForName(const String& name) {
    if (name.equalsIgnoreCase("doggy"))   return new DoggyFace();
    if (name.equalsIgnoreCase("omega"))   return new OmegaFace();
    if (name.equalsIgnoreCase("girly"))   return new GirlyFace();
    if (name.equalsIgnoreCase("demon"))   return new PinkDemonFace();
    if (name.equalsIgnoreCase("cyclops")) return new CyclopsFace();
    return classicFace;  // "classic" / unknown
}

// named palette presets -> (eye, background, cheek) as 0xRRGGBB
static bool themeColors(const String& n, uint32_t& eye, uint32_t& bg, uint32_t& cheek) {
    if (n.equalsIgnoreCase("mint"))   { eye=0x0a2a2a; bg=0xa8f0d0; cheek=0xff9ecf; return true; }
    if (n.equalsIgnoreCase("sunset")) { eye=0x3a1020; bg=0xffb066; cheek=0xff5a7a; return true; }
    if (n.equalsIgnoreCase("mono"))   { eye=0xffffff; bg=0x000000; cheek=0x808080; return true; }
    if (n.equalsIgnoreCase("demon"))  { eye=0xff2020; bg=0xff9ec5; cheek=0xd00040; return true; }
    if (n.equalsIgnoreCase("sky"))    { eye=0x103050; bg=0x9ed0ff; cheek=0xffc0d0; return true; }
    if (n.equalsIgnoreCase("classic")){ eye=0xffffff; bg=0x000000; cheek=0xff8080; return true; }
    return false;
}

static void applyPalette(uint32_t eye, uint32_t bg, uint32_t cheek, bool haveCheek) {
    ColorPalette cp;  // defaults for anything we don't set
    cp.set(COLOR_PRIMARY, to565(eye));
    cp.set(COLOR_BACKGROUND, to565(bg));
    if (haveCheek) cp.set(COLOR_SECONDARY, to565(cheek));
    avatar->setColorPalette(cp);
}

void faceInit(Avatar* av) {
    avatar = av;
    classicFace = avatar->getFace();  // remember the stock face
    faceApplyFromConfig();
}

void faceApplyFromConfig() {
    if (!avatar) return;

    // style
    avatar->setFace(faceForName(cfg.face_style));

    // colors: explicit hex overrides theme; theme name resolves otherwise
    uint32_t eye = 0xffffff, bg = 0x000000, cheek = 0xff8080;
    bool haveCheek = false;
    if (themeColors(cfg.face_theme, eye, bg, cheek)) haveCheek = true;

    uint32_t v;
    if (parseHex(cfg.eye_hex, v))   { eye = v; }
    if (parseHex(cfg.bg_hex, v))    { bg = v; }
    if (parseHex(cfg.cheek_hex, v)) { cheek = v; haveCheek = true; }

    applyPalette(eye, bg, cheek, haveCheek);
    avatar->setBatteryIcon(cfg.show_batt);
}

/* -------------------------------- runtime ------------------------------- */

void faceUpdate() {
    if (!avatar || !cfg.show_batt) return;
    static uint32_t next = 0;
    uint32_t now = millis();
    if (now < next) return;
    next = now + 20000;
    float v = M5StackChan.getBatteryVoltage();
    float mA = M5StackChan.getBatteryCurrent();
    // map ~3.3-4.2V to 0-100%
    int pct = (int)constrain((v - 3.3f) / (4.2f - 3.3f) * 100.0f, 0.0f, 100.0f);
    avatar->setBatteryStatus(mA < -1.0f, pct);  // charging when current is negative
}

void faceWink() {
    if (!avatar) return;
    avatar->setLeftEyeOpenRatio(0.0f);
    delay(220);
    avatar->setLeftEyeOpenRatio(1.0f);
}

/* --------------------------- live setters (API) ------------------------- */

bool faceSetStyle(const String& name) {
    for (const char* s : {"classic", "doggy", "omega", "girly", "demon", "cyclops"}) {
        if (name.equalsIgnoreCase(s)) {
            cfg.face_style = name;
            avatar->setFace(faceForName(name));
            return true;
        }
    }
    return false;
}

bool faceSetTheme(const String& name) {
    uint32_t e, b, c;
    if (!themeColors(name, e, b, c)) return false;
    cfg.face_theme = name;
    cfg.eye_hex = ""; cfg.bg_hex = ""; cfg.cheek_hex = "";  // theme takes over
    applyPalette(e, b, c, true);
    return true;
}

void faceSetColors(const String& eyeHex, const String& bgHex, const String& cheekHex) {
    if (eyeHex.length())   cfg.eye_hex = eyeHex;
    if (bgHex.length())    cfg.bg_hex = bgHex;
    if (cheekHex.length()) cfg.cheek_hex = cheekHex;
    faceApplyFromConfig();
}

void faceSetBatteryIcon(bool on) {
    cfg.show_batt = on;
    avatar->setBatteryIcon(on);
}

String faceStyleList() { return "classic,doggy,omega,girly,demon,cyclops"; }
