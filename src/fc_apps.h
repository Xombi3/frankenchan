// Glance modes shown in the speech bubble: face / clock / weather / timer
#pragma once
#include <Avatar.h>

enum class FCMode { Face, Clock, Weather, Timer, Network };

void appsInit(m5avatar::Avatar* av);
void appsUpdate();                 // call from loop
void appsNextMode(int dir);        // +1 / -1
void appsSetMode(FCMode m);        // jump directly to a mode
FCMode appsMode();
void appsTimerAdd(uint32_t secs);  // add time & (re)start countdown
void appsTimerCancel();
