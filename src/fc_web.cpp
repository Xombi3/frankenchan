#include "fc_web.h"
#include "fc_config.h"
#include "fc_mood.h"
#include "fc_motion.h"
#include "fc_voice.h"
#include "fc_apps.h"
#include "fc_motion.h"
#include "fc_netmon.h"
#include "fc_face.h"
#include "fc_action.h"
#include "fc_ir.h"
#include "fc_nfc.h"
#include "fc_imu.h"
#include "fc_sense.h"
#include "fc_espnow.h"
#include "fc_mqtt.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <M5StackChan.h>

static WebServer server(80);
static DNSServer dns;
static bool portal = false;

bool webPortalActive() { return portal; }

static String htmlPage() {
    String p = R"(<!DOCTYPE html><html><head><meta name=viewport content="width=device-width,initial-scale=1">
<title>FrankenChan Setup</title><style>
body{font-family:system-ui;background:#14141f;color:#eee;max-width:480px;margin:auto;padding:16px}
h1{color:#7fd4ff;font-size:1.4em}h2{color:#ffb3e0;font-size:1.05em;margin-top:22px}
input,textarea,select{width:100%;box-sizing:border-box;padding:8px;margin:4px 0 10px;border-radius:8px;border:1px solid #444;background:#1e1e2e;color:#eee}
button{background:#7fd4ff;color:#123;border:0;border-radius:10px;padding:12px 20px;font-weight:bold;width:100%;font-size:1em}
label{font-size:.85em;color:#aaa}</style></head><body>
<h1>&#9889; FrankenChan Setup</h1><form method=POST action=/save>
<h2>Wi-Fi</h2>
<label>Network name (SSID)</label><input name=ssid value=")" + cfg.wifi_ssid + R"(">
<label>Password</label><input name=pass type=password value=")" + cfg.wifi_pass + R"(">
<h2>Personality</h2>
<label>Robot name</label><input name=name value=")" + cfg.name + R"(">
<label>Personality prompt</label><textarea name=pers rows=3>)" + cfg.personality + R"(</textarea>
<label>Volume (0-255)</label><input name=vol type=number min=0 max=255 value=")" + String(cfg.volume) + R"(">
<h2>Face &amp; Look</h2>
<label>Face style</label>
<select name=fstyle>
<option value=classic>classic</option><option value=doggy>doggy</option>
<option value=omega>omega</option><option value=girly>girly</option>
<option value=demon>demon</option><option value=cyclops>cyclops</option></select>
<label>Color theme</label>
<select name=ftheme>
<option value=classic>classic</option><option value=mint>mint</option>
<option value=sunset>sunset</option><option value=mono>mono</option>
<option value=demon>demon</option><option value=sky>sky</option></select>
<label>Custom eye color (RRGGBB, blank = theme)</label><input name=eye value=")" + cfg.eye_hex + R"(">
<label>Custom background (RRGGBB, blank = theme)</label><input name=bg value=")" + cfg.bg_hex + R"(">
<label>Custom cheek color (RRGGBB, blank = theme)</label><input name=cheek value=")" + cfg.cheek_hex + R"(">
<label><input type=checkbox name=sbatt value=1 )" + String(cfg.show_batt ? "checked" : "") + R"(> Show battery icon</label>
<h2>Clock &amp; Weather</h2>
<label>POSIX timezone (e.g. CET-1CEST,M3.5.0,M10.5.0/3)</label><input name=tz value=")" + cfg.tz + R"(">
<label>City (for weather)</label><input name=city value=")" + cfg.city + R"(">
<h2>Network Monitor</h2>
<label>Internet ping target (IP)</label><input name=ping value=")" + cfg.ping_host + R"(">
<label>Devices to watch (Name:IP, comma separated)</label><input name=devs placeholder="Phone:192.168.1.50,Laptop:192.168.1.60" value=")" + cfg.devices + R"(">
<h2>Screen &amp; Presence</h2>
<label>Brightness (0-255)</label><input name=bright type=number min=5 max=255 value=")" + String(cfg.brightness) + R"(">
<label>Dim after seconds away (0 = never)</label><input name=dim type=number min=0 max=3600 value=")" + String(cfg.dim_after_s) + R"(">
<label>Proximity trigger (raw, lower = more sensitive)</label><input name=prox type=number min=20 max=2000 value=")" + String(cfg.prox_threshold) + R"(">
<label><input type=checkbox name=pgreet value=1 )" + String(cfg.prox_greet ? "checked" : "") + R"(> Greet me when I walk up</label>
<label><input type=checkbox name=cam value=1 )" + String(cfg.camera_on ? "checked" : "") + R"(> Camera motion presence (experimental)</label>

<h2>NFC Tap Routines</h2>
<label>Last tag seen: <b>)" + (nfcLastUid().length() ? nfcLastUid() : String("tap one now")) + R"(</b></label>
<label>Tag map (UID:action, comma separated)</label>
<input name=nfc placeholder="04A2B3C4:timer:25,04FF1122:chat" value=")" + cfg.nfc_tags + R"(">

<h2>Motion Gestures</h2>
<label>When picked up</label><input name=gpick placeholder="chat" value=")" + cfg.gesture_pickup + R"(">
<label>When shaken</label><input name=gshake placeholder="mood:angry" value=")" + cfg.gesture_shake + R"(">
<label>When the desk is tapped</label><input name=gtap placeholder="ir:desk_lamp" value=")" + cfg.gesture_tap + R"(">

<h2>MQTT / Home Assistant</h2>
<label>Broker host (blank = off)</label><input name=mqhost value=")" + cfg.mqtt_host + R"(">
<label>Port</label><input name=mqport type=number value=")" + String(cfg.mqtt_port) + R"(">
<label>Username</label><input name=mquser value=")" + cfg.mqtt_user + R"(">
<label>Password</label><input name=mqpass type=password value=")" + cfg.mqtt_pass + R"(">
<label>Base topic</label><input name=mqtop value=")" + cfg.mqtt_topic + R"(">
<label><input type=checkbox name=mqha value=1 )" + String(cfg.mqtt_ha_discovery ? "checked" : "") + R"(> Home Assistant auto-discovery</label>
<label><input type=checkbox name=espk value=1 )" + String(cfg.espnow_speak ? "checked" : "") + R"(> Speak ESP-NOW sensor alerts out loud</label>

<h2>AI Brain (OpenAI-compatible)</h2>
<label>API base URL</label><input name=api_base value=")" + cfg.api_base + R"(">
<label>API key</label><input name=api_key type=password value=")" + cfg.api_key + R"(">
<label>Chat model</label><input name=chat_m value=")" + cfg.chat_model + R"(">
<label>Speech-to-text model</label><input name=stt_m value=")" + cfg.stt_model + R"(">
<label>Text-to-speech model</label><input name=tts_m value=")" + cfg.tts_model + R"(">
<label>Voice</label><input name=tts_v value=")" + cfg.tts_voice + R"(">
<br><button type=submit>Save &amp; Restart</button></form>
<p style="color:#666;font-size:.8em">FrankenChan custom firmware &middot; REST API: /api/status /say /expression /move /timer /net /scan /face /ir /nfc /sense /espnow /action /announce &middot; live dashboard at /dash</p>
<script>
var _fs=")" + cfg.face_style + R"(",_ft=")" + cfg.face_theme + R"(";
var a=document.querySelector('[name=fstyle]'); if(a)a.value=_fs;
var b=document.querySelector('[name=ftheme]'); if(b)b.value=_ft;
</script></body></html>)";
    return p;
}

static void handleRoot() { server.send(200, "text/html", htmlPage()); }

static void handleSave() {
    cfg.wifi_ssid  = server.arg("ssid");
    cfg.wifi_pass  = server.arg("pass");
    cfg.name       = server.arg("name");
    cfg.personality= server.arg("pers");
    cfg.volume     = constrain(server.arg("vol").toInt(), 0, 255);
    cfg.face_style = server.arg("fstyle").length() ? server.arg("fstyle") : cfg.face_style;
    cfg.face_theme = server.arg("ftheme").length() ? server.arg("ftheme") : cfg.face_theme;
    cfg.eye_hex    = server.arg("eye");
    cfg.bg_hex     = server.arg("bg");
    cfg.cheek_hex  = server.arg("cheek");
    cfg.show_batt  = server.arg("sbatt") == "1" ? 1 : 0;
    if (server.arg("bright").length()) cfg.brightness = constrain(server.arg("bright").toInt(), 5, 255);
    cfg.dim_after_s    = constrain(server.arg("dim").toInt(), 0, 3600);
    if (server.arg("prox").length()) cfg.prox_threshold = constrain(server.arg("prox").toInt(), 20, 2000);
    cfg.prox_greet     = server.arg("pgreet") == "1" ? 1 : 0;
    cfg.camera_on      = server.arg("cam") == "1" ? 1 : 0;
    cfg.nfc_tags       = server.arg("nfc");
    cfg.gesture_pickup = server.arg("gpick");
    cfg.gesture_shake  = server.arg("gshake");
    cfg.gesture_tap    = server.arg("gtap");
    cfg.mqtt_host      = server.arg("mqhost");
    if (server.arg("mqport").length()) cfg.mqtt_port = server.arg("mqport").toInt();
    cfg.mqtt_user      = server.arg("mquser");
    cfg.mqtt_pass      = server.arg("mqpass");
    if (server.arg("mqtop").length()) cfg.mqtt_topic = server.arg("mqtop");
    cfg.mqtt_ha_discovery = server.arg("mqha") == "1" ? 1 : 0;
    cfg.espnow_speak   = server.arg("espk") == "1" ? 1 : 0;
    cfg.tz         = server.arg("tz");
    if (server.arg("city") != cfg.city) { cfg.lat = 0; cfg.lon = 0; }  // re-geocode
    cfg.city       = server.arg("city");
    cfg.ping_host  = server.arg("ping").length() ? server.arg("ping") : cfg.ping_host;
    cfg.devices    = server.arg("devs");
    cfg.api_base   = server.arg("api_base");
    cfg.api_key    = server.arg("api_key");
    cfg.chat_model = server.arg("chat_m");
    cfg.stt_model  = server.arg("stt_m");
    cfg.tts_model  = server.arg("tts_m");
    cfg.tts_voice  = server.arg("tts_v");
    cfg.save();
    server.send(200, "text/html",
        "<html><body style='font-family:system-ui;background:#14141f;color:#7fd4ff;text-align:center;padding-top:40px'>"
        "<h1>Saved! Restarting...</h1><p style='color:#aaa'>Reconnect to your normal Wi-Fi.</p></body></html>");
    delay(800);
    ESP.restart();
}

/* ------------------------------- REST API ------------------------------- */

static void apiStatus() {
    String j = "{\"name\":\"" + cfg.name + "\",\"mood\":\"" + moodName(moodCurrent()) +
               "\",\"battery_v\":" + String(M5StackChan.getBatteryVoltage(), 2) +
               ",\"battery_ma\":" + String(M5StackChan.getBatteryCurrent(), 1) +
               ",\"uptime_s\":" + String(millis() / 1000) +
               ",\"ip\":\"" + WiFi.localIP().toString() + "\"" +
               ",\"mac\":\"" + WiFi.macAddress() + "\"" +
               ",\"mqtt\":" + String(mqttConnected() ? "true" : "false") + "}";
    server.send(200, "application/json", j);
}

static void apiSay() {
    String t = server.arg("text");
    if (!t.length()) { server.send(400, "text/plain", "need ?text="); return; }
    server.send(200, "text/plain", "ok");
    speakText(t);
}

static void apiExpression() {
    Mood m;
    if (moodFromName(server.arg("e"), m)) {
        moodSet(m, 8000); moodChirp(m);
        server.send(200, "text/plain", "ok");
    } else {
        server.send(400, "text/plain", "e = happy|excited|sad|angry|doubt|sleepy|neutral");
    }
}

static void apiMove() {
    float x = constrain(server.arg("x").toFloat(), -1.0f, 1.0f);
    float y = constrain(server.arg("y").toFloat(), -1.0f, 1.0f);
    gestureLook(x, y);
    server.send(200, "text/plain", "ok");
}

static void apiTimer() {
    long mins = server.arg("min").toInt();
    if (mins > 0) { appsTimerAdd(mins * 60); server.send(200, "text/plain", "ok"); }
    else if (server.arg("cancel") == "1") { appsTimerCancel(); server.send(200, "text/plain", "ok"); }
    else server.send(400, "text/plain", "need ?min=N or ?cancel=1");
}

static void addCors() { server.sendHeader("Access-Control-Allow-Origin", "*"); }

static void apiFace() {
    bool ok = false;
    if (server.hasArg("style"))   ok |= faceSetStyle(server.arg("style"));
    if (server.hasArg("theme"))   ok |= faceSetTheme(server.arg("theme"));
    if (server.hasArg("eye") || server.hasArg("bg") || server.hasArg("cheek")) {
        faceSetColors(server.arg("eye"), server.arg("bg"), server.arg("cheek"));
        ok = true;
    }
    if (server.hasArg("battery")) { faceSetBatteryIcon(server.arg("battery") == "1"); ok = true; }
    if (server.hasArg("wink"))    { faceWink(); ok = true; }
    if (server.arg("save") == "1") cfg.save();
    addCors();
    if (ok) server.send(200, "text/plain", "ok");
    else    server.send(400, "text/plain",
        "params: style=" + faceStyleList() + " theme=mint|sunset|mono|demon|sky|classic eye=RRGGBB bg= cheek= battery=0|1 wink=1 save=1");
}

static void apiAction() {
    String a = server.arg("do");
    addCors();
    if (!a.length()) { server.send(400, "text/plain", "need ?do=<action>, see /api/actions"); return; }
    bool ok = runAction(a);
    server.send(ok ? 200 : 400, "text/plain", ok ? "ok" : "unknown action");
}

static void apiActions() {
    addCors();
    server.send(200, "text/plain",
        "chat | say:TEXT | mood:happy | face:doggy | theme:mint | wink | ir:NAME | "
        "timer:25 | timer:cancel | mode:network | scan | nod | shake | tilt | wiggle");
}

static void apiIr() {
    addCors();
    if (server.hasArg("send"))  { server.send(irSend(server.arg("send")) ? 200 : 404,
                                              "text/plain", "ir"); return; }
    if (server.hasArg("learn")) { irLearnStart(server.arg("learn"));
                                  server.send(200, "text/plain", "press the button now"); return; }
    if (server.hasArg("forget")){ server.send(irForget(server.arg("forget")) ? 200 : 404,
                                              "text/plain", "ir"); return; }
    if (server.arg("cancel") == "1") { irLearnCancel(); server.send(200, "text/plain", "ok"); return; }
    server.send(200, "application/json", irJson());
}

static void apiNfc()    { addCors(); server.send(200, "application/json", nfcJson()); }
static void apiSense()  { addCors(); server.send(200, "application/json", senseJson()); }
static void apiEspnow() { addCors(); server.send(200, "application/json", espnowJson()); }

static void apiAnnounce() {
    // generic webhook: anything can give the robot a voice
    String text = server.hasArg("text") ? server.arg("text") : server.arg("plain");
    addCors();
    if (!text.length()) { server.send(400, "text/plain", "need ?text= or a POST body"); return; }
    if (server.hasArg("mood")) runAction("mood:" + server.arg("mood"));
    server.send(200, "text/plain", "ok");
    gestureNod();
    speakText(text);
}

static void handleDash() {
    static const char DASH[] PROGMEM = R"HTML(<!DOCTYPE html><html><head>
<meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>FrankenChan Network</title><style>
body{font-family:system-ui;background:#0f0f1a;color:#e8e8f0;max-width:560px;margin:auto;padding:18px}
h1{color:#7fd4ff;font-size:1.4em}.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.box{background:#1a1a2b;border:1px solid #2c2c44;border-radius:12px;padding:14px}
.k{color:#8aa;font-size:.8em}.v{font-size:1.5em;font-weight:bold}
.up{color:#7fffa0}.down{color:#ff7f7f}
table{width:100%;border-collapse:collapse;margin-top:6px}td{padding:5px;border-bottom:1px solid #26263c;font-size:.9em}
.dot{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:6px}
.on{background:#7fffa0}.off{background:#555}
button{background:#7fd4ff;color:#123;border:0;border-radius:8px;padding:8px 14px;font-weight:bold;cursor:pointer}
.bar{height:8px;background:#26263c;border-radius:4px;overflow:hidden;margin-top:6px}
.fill{height:100%;background:#7fd4ff}</style></head><body>
<h1>&#128225; FrankenChan Network</h1>
<div class=grid>
<div class=box><div class=k>Signal</div><div class=v id=sig>--</div><div class=bar><div class=fill id=sigbar></div></div></div>
<div class=box><div class=k>Internet</div><div class=v id=net>--</div><div id=lat class=k></div></div>
</div>
<div class=box style=margin-top:12px><div class=k>Devices</div><table id=devs></table></div>
<div class=grid style=margin-top:12px>
<div class=box><div class=k>Desk presence</div><div class=v id=pres>--</div><div class=k id=presd></div></div>
<div class=box><div class=k>MQTT</div><div class=v id=mq>--</div><div class=k id=mac></div></div>
</div>
<div class=box style=margin-top:12px><div class=k>ESP-NOW sensor nodes</div><table id=nodes></table></div>
<div class=box style=margin-top:12px><div class=k>Learned IR commands</div><table id=ir></table></div>
<div class=box style=margin-top:12px><div class=k>Nearby networks</div>
<button onclick=doScan()>Rescan</button><table id=aps></table></div>
<p class=k id=meta></p>
<script>
async function load(){try{
 let r=await fetch('/api/net');let d=await r.json();
 document.getElementById('sig').textContent=d.connected?d.quality+'% ('+d.rssi+'dBm)':'Offline';
 document.getElementById('sigbar').style.width=(d.quality||0)+'%';
 let n=document.getElementById('net');
 n.textContent=d.internet_up?'UP':'DOWN';n.className='v '+(d.internet_up?'up':'down');
 document.getElementById('lat').textContent=d.internet_up?(d.latency_ms+'ms, '+d.loss_pct+'% loss'):'';
 let dv='';for(const x of d.devices){dv+='<tr><td><span class="dot '+(x.online?'on':'off')+'"></span>'+x.name+'</td><td>'+x.ip+'</td><td>'+(x.online?x.rtt+'ms':'away')+'</td></tr>';}
 document.getElementById('devs').innerHTML=dv||'<tr><td class=k>none tracked</td></tr>';
 let ap='';for(const x of d.scan.sort((a,b)=>b.rssi-a.rssi)){ap+='<tr><td>'+(x.ssid||'(hidden)')+'</td><td>ch'+x.ch+'</td><td>'+x.rssi+'dBm</td></tr>';}
 document.getElementById('aps').innerHTML=ap||'<tr><td class=k>tap Rescan</td></tr>';
 document.getElementById('meta').textContent='SSID '+d.ssid+' - IP '+d.ip+' - updated '+new Date().toLocaleTimeString();
 try{
  let sr=await(await fetch('/api/sense')).json();
  let p=document.getElementById('pres');
  p.textContent=(sr.near||sr.motion)?'HERE':'AWAY';p.className='v '+((sr.near||sr.motion)?'up':'');
  document.getElementById('presd').textContent='prox '+sr.proximity+', light '+sr.light+(sr.camera?', cam on':'');
  let st=await(await fetch('/api/status')).json();
  let m=document.getElementById('mq');m.textContent=st.mqtt?'connected':'off';m.className='v '+(st.mqtt?'up':'');
  document.getElementById('mac').textContent='MAC '+st.mac;
  let en=await(await fetch('/api/espnow')).json();let nh='';
  for(const x of en.nodes){nh+='<tr><td>'+x.id+'</td><td>'+x.kind+'</td><td>'+x.value+'</td><td>'+x.age_s+'s ago</td></tr>';}
  document.getElementById('nodes').innerHTML=nh||'<tr><td class=k>no nodes yet</td></tr>';
  let ir=await(await fetch('/api/ir')).json();let ih='';
  for(const c of ir.commands){ih+='<tr><td>'+c.name+'</td><td>'+c.proto+'</td><td><button onclick="fetch(\'/api/ir?send='+c.name+'\')">send</button></td></tr>';}
  document.getElementById('ir').innerHTML=ih||'<tr><td class=k>none learned</td></tr>';
 }catch(e){}
}catch(e){document.getElementById('meta').textContent='Cannot reach FrankenChan';}}
function doScan(){fetch('/api/scan');setTimeout(load,4000);}
load();setInterval(load,5000);
</script></body></html>)HTML";
    server.send_P(200, "text/html", DASH);
}

static void apiNet() {
    addCors();
    server.send(200, "application/json", netJson());
}

static void apiScan() {
    netRequestScan();
    addCors();
    server.send(200, "text/plain", "scanning");
}

/* -------------------------------- start --------------------------------- */

void webStart(bool apMode) {
    portal = apMode;
    if (apMode) {
        WiFi.mode(WIFI_AP);
        String apName = "FrankenChan-" + String((uint32_t)(ESP.getEfuseMac() & 0xFFFF), HEX);
        WiFi.softAP(apName.c_str(), "stackchan");
        dns.start(53, "*", WiFi.softAPIP());
        server.onNotFound(handleRoot);  // captive portal
    } else {
        MDNS.begin("frankenchan");
        MDNS.addService("http", "tcp", 80);
    }
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/api/status", apiStatus);
    server.on("/api/say", apiSay);
    server.on("/api/expression", apiExpression);
    server.on("/api/move", apiMove);
    server.on("/api/timer", apiTimer);
    server.on("/api/net", apiNet);
    server.on("/api/scan", apiScan);
    server.on("/api/face", apiFace);
    server.on("/api/action", apiAction);
    server.on("/api/actions", apiActions);
    server.on("/api/ir", apiIr);
    server.on("/api/nfc", apiNfc);
    server.on("/api/sense", apiSense);
    server.on("/api/espnow", apiEspnow);
    server.on("/api/announce", apiAnnounce);
    server.on("/dash", handleDash);
    server.begin();
}

void webUpdate() {
    if (portal) dns.processNextRequest();
    server.handleClient();
}
