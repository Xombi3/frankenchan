// Network monitor: own RSSI, internet health (latency/loss), nearby AP scan,
// tracked-device presence. Runs in a background task; mood reacts to outages.
#pragma once
#include <Arduino.h>
#include <vector>

struct WifiAP { String ssid; int rssi; int channel; };
struct NetDevice { String name; String ip; bool online; int rtt; };

void netmonInit();
void netmonUpdate();               // call from loop (cheap; work happens in task)

// snapshot getters (safe to read from loop)
int   netRssi();                   // dBm, 0 if offline
int   netQuality();                // 0-100 %
bool  netInternetUp();
int   netLatencyMs();              // internet ping, -1 if down
int   netLossPct();                // rolling packet loss %
String netSummaryLine();           // one-line status for the face bubble

std::vector<NetDevice> netDevices();
std::vector<WifiAP>    netScanResults();
void  netRequestScan();            // trigger an AP scan (async)
bool  netScanBusy();
String netJson();                  // full state as JSON (for API/dashboard)
