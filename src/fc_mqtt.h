// MQTT bridge with Home Assistant auto-discovery.
//   subscribe: frankenchan/cmd        -> any action from fc_action.h
//              frankenchan/say        -> speak the payload
//              frankenchan/mood       -> set expression
//   publish:   frankenchan/state      -> JSON status every 30s
//              frankenchan/event      -> nfc taps, gestures, sensor alerts
#pragma once
#include <Arduino.h>

void mqttInit();
void mqttUpdate();                                   // call from loop
bool mqttConnected();
void mqttPublishEvent(const String& kind, const String& detail);
