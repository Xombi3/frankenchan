// Setup portal (AP mode) + settings page + REST API when online
#pragma once
#include <Arduino.h>

void webStart(bool apMode);
void webUpdate();          // call from loop
bool webPortalActive();
