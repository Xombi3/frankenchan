// Face customizer: swappable face styles, color themes, battery icon, winks.
// All calls run on the main thread (boot + web API), so they're avatar-safe.
#pragma once
#include <Avatar.h>

void faceInit(m5avatar::Avatar* av);   // build face styles, apply saved look
void faceApplyFromConfig();            // re-read cfg and apply style+colors
void faceUpdate();                     // feed live battery status to the icon
void faceWink();                       // quick one-eye wink

bool faceSetStyle(const String& name); // classic|doggy|omega|girly|demon|cyclops
bool faceSetTheme(const String& name); // mint|sunset|mono|demon|sky|classic
void faceSetColors(const String& eyeHex, const String& bgHex, const String& cheekHex);
void faceSetBatteryIcon(bool on);
String faceStyleList();                // comma list for the web UI
