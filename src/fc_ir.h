// Universal IR remote: learn codes from your existing remotes, replay by name.
// Hardware: StackChan body IR TX = GPIO5, RX = GPIO10.
#pragma once
#include <Arduino.h>
#include <vector>

struct IRCommand { String name; String proto; uint64_t code; uint16_t bits; };

void irInit();
void irUpdate();                       // call from loop (services learn mode)

bool irSend(const String& name);       // replay a stored command
bool irLearnStart(const String& name); // capture the next button press as <name>
bool irLearning();
void irLearnCancel();

std::vector<IRCommand> irList();
bool irForget(const String& name);
String irJson();
