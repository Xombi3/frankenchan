// Mood engine: expressions, RGB LEDs, idle behaviors, sound chirps
#pragma once
#include <Avatar.h>

enum class Mood { Neutral, Happy, Sleepy, Doubt, Sad, Angry, Excited };

void moodInit(m5avatar::Avatar* av);
void moodSet(Mood m, uint32_t holdMs = 4000);   // temporary mood, decays to neutral
void moodUpdate();                              // call from loop
void moodChirp(Mood m);                         // play a little sound for a mood
Mood moodCurrent();
const char* moodName(Mood m);
bool moodFromName(const String& s, Mood& out);
