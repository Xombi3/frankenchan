// IMU gestures: pick-up, shake, desk tap, and "someone moved me while away"
#pragma once
#include <Arduino.h>

void imuInit();
void imuUpdate();          // call from loop
bool imuHeldUp();          // currently picked up
bool imuWasMoved();        // consumed flag: moved since last check
String imuJson();
