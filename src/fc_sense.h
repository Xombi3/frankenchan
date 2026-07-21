// Presence sensing:
//  - LTR-553 proximity/ambient light (wake on approach, auto-dim when away)
//  - Camera frame-diff motion detection ("are you at your desk")
// Both are optional and fail soft. The camera shares the internal I2C bus during
// init only, so it is initialised once at boot and left alone afterwards.
#pragma once
#include <Arduino.h>

void senseInit();
void senseUpdate();          // call from loop

bool  senseNear();           // someone close right now (proximity)
uint16_t senseProximity();   // raw PS value
uint16_t senseLight();       // relative ambient light
bool  senseMotion();         // camera saw movement recently
bool  senseCameraReady();
uint32_t senseLastPresenceMs();  // ms since any presence signal
String senseJson();
