// ESP-NOW sensor mesh: cheap battery nodes (door, temp, soil, mailbox) report
// straight to StackChan with no Wi-Fi router involved. See extras/sensor_node/.
#pragma once
#include <Arduino.h>
#include <vector>

struct SensorNode {
    String   id;         // node name, e.g. "front_door"
    String   kind;       // "door" | "temp" | "motion" | "soil" | ...
    float    value;
    String   text;       // optional human string
    int      bearing;    // -128..127 degrees; head turns this way on alert
    uint32_t lastSeen;
    int      rssi;
};

void espnowInit();
void espnowUpdate();                  // call from loop (handles queued packets)
std::vector<SensorNode> espnowNodes();
String espnowJson();
