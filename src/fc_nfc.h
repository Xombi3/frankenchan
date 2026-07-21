// NFC tap routines: map tag UIDs to actions (see fc_action.h for the action language)
#pragma once
#include <Arduino.h>

void nfcInit();
void nfcUpdate();            // call from loop
bool nfcAvailable();
String nfcLastUid();         // most recently seen tag, for easy pairing in the UI
uint32_t nfcLastSeenMs();
String nfcJson();
