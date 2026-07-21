// Voice chat: mic record -> STT -> LLM -> TTS -> speaker, with lipsync
#pragma once
#include <Avatar.h>

void voiceInit(m5avatar::Avatar* av);
bool voiceConfigured();
void voiceChatRound();                 // one full conversation turn (blocking)
void speakText(const String& text);   // TTS + play only (used by web API)
