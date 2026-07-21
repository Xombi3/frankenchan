// Head motion: idle sway task + expressive gestures (units: 10 = 1 degree)
//
// Safety: M5Stack's own factory firmware limits pitch to 30..870 (not the
// 0..900 the BSP permits) and enables servo stall protection, which tells us the
// physical end stops sit just outside that band. Every command here is clamped
// to the factory envelope so we never drive the head into a stop and stall it.
#pragma once
#include <Arduino.h>

static const int FC_YAW_MIN   = -1280;   // -128 deg
static const int FC_YAW_MAX   =  1280;   //  128 deg
static const int FC_PITCH_MIN =    30;   //    3 deg  (factory safe floor)
static const int FC_PITCH_MAX =   870;   //   87 deg  (factory safe ceiling)

void motionSafeMove(int yaw, int pitch, int speed);   // clamped move
void gestureLookBearing(int degrees);                 // turn toward a sensor

void motionInit();
void motionIdleEnable(bool en);
void gestureNod();
void gestureShake();
void gestureTilt();       // curious tilt
void gestureWiggle();     // excited full spin-ish wiggle
void gestureLook(float nx, float ny);  // normalized -1..1
