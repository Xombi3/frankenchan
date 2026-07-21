// One action language shared by NFC tags, IMU gestures, ESP-NOW nodes, MQTT and REST.
//   chat              start a voice conversation
//   say:Hello there   speak text
//   mood:happy        set expression
//   face:doggy        swap face style
//   theme:mint        swap color theme
//   wink
//   ir:tv_power       replay a learned IR command
//   timer:25          start a 25 minute timer
//   timer:cancel
//   mode:network      jump to a glance mode (face|clock|weather|timer|network)
//   scan              rescan nearby wifi
//   nod / shake / tilt / wiggle
#pragma once
#include <Arduino.h>

bool runAction(const String& action);
