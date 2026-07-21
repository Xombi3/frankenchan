#include "fc_voice.h"
#include "fc_config.h"
#include "fc_mood.h"
#include "fc_motion.h"
#include <M5StackChan.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>

using namespace m5avatar;

static Avatar* avatar = nullptr;
static std::vector<String> histRole, histText;   // chat history (paired)
static char bubble[96];

static const uint32_t SR = 16000;
static const uint32_t MAX_REC_SEC = 10;

void voiceInit(Avatar* av) { avatar = av; }

bool voiceConfigured() {
    return WiFi.status() == WL_CONNECTED && cfg.api_key.length() > 4;
}

static void setBubble(const char* s) {
    strncpy(bubble, s, sizeof(bubble) - 1);
    bubble[sizeof(bubble) - 1] = 0;
    if (avatar) avatar->setSpeechText(bubble);
}

/* ------------------------------ recording ------------------------------- */

static size_t recordAudio(int16_t* buf, size_t maxSamples) {
    M5.Speaker.end();
    M5.Mic.begin();
    delay(80);
    setBubble("Listening...");
    moodSet(Mood::Doubt, 15000);

    size_t got = 0;
    const size_t chunk = 512;
    uint32_t start = millis();
    // small pre-roll flush
    M5.Mic.record(buf, chunk, SR);
    while (M5.Mic.isRecording()) delay(1);

    while (got + chunk <= maxSamples) {
        M5.Mic.record(buf + got, chunk, SR);
        while (M5.Mic.isRecording()) delay(1);
        got += chunk;
        M5StackChan.update();
        if (M5StackChan.TouchSensor.wasClicked()) break;          // tap again = done
        if (millis() - start > MAX_REC_SEC * 1000UL) break;
    }
    M5.Mic.end();
    delay(50);
    M5.Speaker.begin();
    M5.Speaker.setVolume(cfg.volume);
    return got;
}

static void wavHeader(uint8_t* h, uint32_t dataLen) {
    uint32_t byteRate = SR * 2;
    uint32_t riffLen = dataLen + 36;
    memcpy(h, "RIFF", 4); memcpy(h + 4, &riffLen, 4);
    memcpy(h + 8, "WAVEfmt ", 8);
    uint32_t fmtLen = 16; uint16_t fmt = 1, ch = 1, align = 2, bits = 16;
    memcpy(h + 16, &fmtLen, 4); memcpy(h + 20, &fmt, 2); memcpy(h + 22, &ch, 2);
    memcpy(h + 24, &SR, 4); memcpy(h + 28, &byteRate, 4);
    memcpy(h + 32, &align, 2); memcpy(h + 34, &bits, 2);
    memcpy(h + 36, "data", 4); memcpy(h + 40, &dataLen, 4);
}

/* --------------------------------- STT ---------------------------------- */

static String sttTranscribe(const int16_t* samples, size_t n) {
    const char* boundary = "----frankenchan7482";
    uint32_t dataLen = n * 2;

    String head = String("--") + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n\r\n" + cfg.stt_model + "\r\n"
        "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"a.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n";
    String tail = String("\r\n--") + boundary + "--\r\n";

    size_t total = head.length() + 44 + dataLen + tail.length();
    uint8_t* body = (uint8_t*)heap_caps_malloc(total, MALLOC_CAP_SPIRAM);
    if (!body) return "";
    size_t off = 0;
    memcpy(body, head.c_str(), head.length()); off += head.length();
    wavHeader(body + off, dataLen); off += 44;
    memcpy(body + off, samples, dataLen); off += dataLen;
    memcpy(body + off, tail.c_str(), tail.length()); off += tail.length();

    WiFiClientSecure client; client.setInsecure();
    HTTPClient http;
    http.setTimeout(30000);
    http.begin(client, cfg.api_base + "/audio/transcriptions");
    http.addHeader("Authorization", "Bearer " + cfg.api_key);
    http.addHeader("Content-Type", String("multipart/form-data; boundary=") + boundary);
    int code = http.POST(body, off);
    String out;
    if (code == 200) {
        JsonDocument doc;
        if (!deserializeJson(doc, http.getString())) out = doc["text"].as<String>();
    }
    http.end();
    free(body);
    return out;
}

/* --------------------------------- LLM ---------------------------------- */

static String chatComplete(const String& userText) {
    JsonDocument doc;
    JsonArray msgs = doc["messages"].to<JsonArray>();
    JsonObject sys = msgs.add<JsonObject>();
    sys["role"] = "system";
    sys["content"] = cfg.personality +
        " Your name is " + cfg.name + ". Reply in at most 55 words, plain text, no emoji."
        " Start your reply with exactly one mood tag from this list:"
        " [happy] [excited] [sad] [angry] [doubt] [sleepy] [neutral]";
    for (size_t i = 0; i < histRole.size(); i++) {
        JsonObject m = msgs.add<JsonObject>();
        m["role"] = histRole[i]; m["content"] = histText[i];
    }
    JsonObject u = msgs.add<JsonObject>();
    u["role"] = "user"; u["content"] = userText;
    doc["model"] = cfg.chat_model;
    doc["max_tokens"] = 200;

    String body; serializeJson(doc, body);

    WiFiClientSecure client; client.setInsecure();
    HTTPClient http;
    http.setTimeout(30000);
    http.begin(client, cfg.api_base + "/chat/completions");
    http.addHeader("Authorization", "Bearer " + cfg.api_key);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    String reply;
    if (code == 200) {
        JsonDocument rd;
        if (!deserializeJson(rd, http.getString()))
            reply = rd["choices"][0]["message"]["content"].as<String>();
    }
    http.end();

    if (reply.length()) {
        histRole.push_back("user");      histText.push_back(userText);
        histRole.push_back("assistant"); histText.push_back(reply);
        while (histRole.size() > 8) {    // keep last 4 exchanges
            histRole.erase(histRole.begin()); histText.erase(histText.begin());
        }
    }
    return reply;
}

/* --------------------------------- TTS ---------------------------------- */

static bool ttsAndPlay(const String& text) {
    JsonDocument doc;
    doc["model"] = cfg.tts_model;
    doc["voice"] = cfg.tts_voice;
    doc["input"] = text;
    doc["response_format"] = "wav";
    String body; serializeJson(doc, body);

    WiFiClientSecure client; client.setInsecure();
    HTTPClient http;
    http.setTimeout(45000);
    http.begin(client, cfg.api_base + "/audio/speech");
    http.addHeader("Authorization", "Bearer " + cfg.api_key);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    if (code != 200) { http.end(); return false; }

    const size_t cap = 3 * 1024 * 1024;
    uint8_t* wav = (uint8_t*)heap_caps_malloc(cap, MALLOC_CAP_SPIRAM);
    if (!wav) { http.end(); return false; }
    WiFiClient* stream = http.getStreamPtr();
    size_t len = 0;
    uint32_t last = millis();
    while (http.connected() && len < cap) {
        size_t avail = stream->available();
        if (avail) {
            len += stream->readBytes(wav + len, min(avail, cap - len));
            last = millis();
        } else {
            if (millis() - last > 5000) break;
            delay(5);
        }
    }
    http.end();
    if (len < 100) { free(wav); return false; }

    M5.Speaker.setVolume(cfg.volume);
    M5.Speaker.playWav(wav, len);
    // lipsync while playing
    while (M5.Speaker.isPlaying()) {
        if (avatar) avatar->setMouthOpenRatio(0.2f + (esp_random() % 70) / 100.0f);
        delay(60 + (esp_random() % 60));
    }
    if (avatar) avatar->setMouthOpenRatio(0);
    free(wav);
    return true;
}

void speakText(const String& text) {
    moodSet(Mood::Happy, 6000);
    ttsAndPlay(text);
}

/* ------------------------------ full round ------------------------------ */

void voiceChatRound() {
    if (!voiceConfigured()) {
        setBubble("Set me up at frankenchan.local");
        moodSet(Mood::Sad, 3000); moodChirp(Mood::Sad);
        delay(2500); setBubble("");
        return;
    }
    motionIdleEnable(false);

    size_t maxSamples = SR * MAX_REC_SEC;
    int16_t* buf = (int16_t*)heap_caps_malloc(maxSamples * 2, MALLOC_CAP_SPIRAM);
    if (!buf) { motionIdleEnable(true); return; }

    M5.Speaker.tone(1500, 60);  delay(80);           // "I'm listening" beep
    size_t n = recordAudio(buf, maxSamples);

    setBubble("Hmm...");
    gestureTilt();
    String text = n > SR / 2 ? sttTranscribe(buf, n) : "";
    free(buf);

    if (!text.length()) {
        setBubble("I didn't catch that!");
        moodSet(Mood::Doubt, 2500); moodChirp(Mood::Doubt);
        delay(2000); setBubble("");
        motionIdleEnable(true);
        return;
    }

    String reply = chatComplete(text);
    if (!reply.length()) {
        setBubble("Brain freeze... try again?");
        moodSet(Mood::Sad, 2500);
        delay(2000); setBubble("");
        motionIdleEnable(true);
        return;
    }

    // parse leading [mood] tag
    Mood m = Mood::Happy;
    if (reply.startsWith("[")) {
        int e = reply.indexOf(']');
        if (e > 0 && e < 12) {
            String tag = reply.substring(1, e);
            moodFromName(tag, m);
            reply = reply.substring(e + 1);
            reply.trim();
        }
    }
    moodSet(m, 12000);
    setBubble("");
    if (m == Mood::Happy || m == Mood::Excited) gestureNod();
    if (m == Mood::Sad || m == Mood::Angry) gestureShake();

    if (!ttsAndPlay(reply)) {
        // no TTS? show the text instead, in chunks
        for (unsigned i = 0; i < reply.length(); i += 60) {
            setBubble(reply.substring(i, min(i + 60U, reply.length())).c_str());
            delay(2600);
        }
    }
    setBubble("");
    motionIdleEnable(true);
}
