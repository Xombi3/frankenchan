# ⚡ FrankenChan

**Custom firmware for the official M5Stack StackChan (SKU K151, CoreS3) — a complete package.**
A living desk robot with real moods, head motion, AI voice chat, useful glances, and an IoT REST API. Flash it from your browser in two minutes.

## What it does

**A face that feels alive.** Seven moods (neutral, happy, excited, doubt, sleepy, sad, angry) with matching RGB body lighting, automatic blinking, random idle quirks, and a sleepy face when the battery runs low. Poke its cheeks and it giggles and tilts its head.

**A head that moves like it means it.** Idle glancing around, plus a gesture library — nods when it agrees, shakes when it's upset, tilts when it's thinking, and does a full-body wiggle when excited.

**AI voice chat.** Tap its back, talk, tap again — it transcribes your speech, thinks, and answers out loud with lip-sync. The LLM controls its own mood per reply. Works with any OpenAI-compatible API (OpenAI, local servers like Ollama/LM Studio behind a proxy, etc.). No key? It still shows replies as text bubbles — and everything else works fully offline-from-cloud.

**Useful glances.** Swipe its back panel to cycle: **Face → Clock → Weather → Timer**. Weather is free (Open-Meteo, no API key), clock syncs via NTP, and the kitchen timer rings with a light show and a happy dance.

**Watches your network.** A built-in monitor tracks its own Wi-Fi signal strength, pings the internet for latency and packet loss, scans nearby access points like a mini Wi-Fi analyzer, and pings devices you care about (phone, laptop) to see who's home. When the internet drops it pulls a sad face and chirps; when it comes back it cheers. Everything shows up in the new **Network** glance screen (swipe to it) and in a live dashboard the robot serves at **http://frankenchan.local/dash** — signal bar, up/down status, device presence, and a nearby-networks table, auto-refreshing. It's also all on the REST API for Home Assistant.

**A face you can restyle.** Six face styles (classic, doggy, omega, girly, demon, and a custom cyclops), named color themes (mint, sunset, mono, demon, sky, classic), and fully custom eye / background / cheek colors by hex. Toggle the battery icon, and it winks when you poke its right cheek. Change any of it from the setup page, or live over the API without a reflash:

```
GET /api/face?style=doggy                     → swap face style
GET /api/face?theme=sunset                     → apply a color theme
GET /api/face?eye=00FF88&bg=101020&cheek=FF80A0 → custom colors
GET /api/face?battery=0                         → hide battery icon
GET /api/face?wink=1                            → wink now
GET /api/face?...&save=1                         → also persist across reboots
```

**Universal IR remote.** The body has an IR blaster and receiver, so it can learn codes straight from your existing remotes and replay them by name. `GET /api/ir?learn=tv_power`, point your remote at it, press the button — done, stored in flash. After that `ir:tv_power` is an action you can trigger by voice, NFC tag, MQTT, a desk tap, or a button on the dashboard. Your dumb appliances are now scriptable.

**NFC tap routines.** Stick tags on desk objects and map each UID to an action: tap a coaster to start a 25-minute timer, tap a badge to begin a conversation, tap a coin to kill the lights. The setup page shows the last tag it saw, so pairing is copy-paste.

**Motion gestures.** The 9-axis IMU gives it body awareness: pick it up and it stops flailing its servos and (optionally) starts listening, shake it to cancel, tap the desk beside it to fire an action. It also knows if it was moved while you were away.

**Knows when you're there.** The proximity sensor wakes and greets it when you walk up, and dims the screen when you've been gone a while (big battery win). Optional camera frame-diff motion detection adds "is anyone actually at the desk" without sending a single image anywhere — it's all on-device, and off by default.

**ESP-NOW sensor mesh.** Flash a spare ESP32 straight from the same web flasher to make a battery-powered door/temp/soil/mailbox sensor that reports *directly* to StackChan — no router, no broker, no cloud. Nodes broadcast and sweep channels, so there is no MAC address to copy and no channel to configure; each one names itself through its own setup portal. Every node has a **bearing**, so when the front door opens the robot physically turns its head that way, pulls a curious face, and reads the alert out loud. See `extras/sensor_node/`.

**MQTT + Home Assistant.** Point it at a broker and it auto-discovers into Home Assistant as a device with mood, battery, Wi-Fi signal, internet latency and desk-presence sensors, plus a text entity so HA can make it talk. It subscribes to `frankenchan/cmd` (any action), `frankenchan/say` and `frankenchan/mood`, and publishes NFC taps, gestures and sensor alerts to `frankenchan/event`. That one bridge is how you'd wire in calendar nags, CI build status, delivery alerts, or anything else your automation stack already knows about.

**IoT built in.** A REST API for Home Assistant, curl, or anything else:

```
GET /api/status                     → {"name":..,"mood":..,"battery_v":..}
GET /api/say?text=Dinner+is+ready   → speaks out loud (TTS)
GET /api/expression?e=happy         → set mood (happy|excited|sad|angry|doubt|sleepy|neutral)
GET /api/move?x=0.5&y=-0.2          → look at a point (normalized -1..1)
GET /api/timer?min=10               → start/extend timer;  ?cancel=1 to stop
GET /api/net                        → JSON: signal, internet up/latency/loss, devices, scan
GET /api/scan                       → trigger a nearby-network scan
GET /api/face?style=doggy&theme=mint&save=1   → change the face live (see below)
GET /api/action?do=timer:25         → run any action (see /api/actions for the list)
GET /api/announce?text=Dinner!      → look up, nod, and say it (generic webhook)
GET /api/ir                         → learned IR commands;  ?send=tv_power  ?learn=name  ?forget=name
GET /api/nfc                        → NFC state + last tag UID seen
GET /api/sense                      → proximity, ambient light, camera motion, presence
GET /api/espnow                     → ESP-NOW sensor nodes and their latest values
```

## Flashing — everything from one page

The web flasher handles **every** firmware in this project: the robot itself and sensor nodes for ESP32, ESP32-S3 and ESP32-C3. Pick your device, plug it in, click install. No toolchain, no drivers, no command line, no editing source.

1. Push this repo to GitHub and enable **Settings → Pages → Source: GitHub Actions**
2. The workflow builds all four firmwares and publishes the flasher at `https://<you>.github.io/<repo>/`
3. Open it in Chrome or Edge, connect over USB-C, hit **Install**

Nothing needs configuring before you flash — both the robot and the nodes set themselves up over their own Wi-Fi portal on first boot.

### Building locally instead

```bash
pip install platformio
pio run                                  # the robot
pio run -t upload                        # flash it over USB
cd extras/sensor_node && pio run         # all three sensor node variants
```

## First-boot setup

FrankenChan opens a Wi-Fi network **`FrankenChan-xxxx`** (password **`stackchan`**). Join it and the setup page appears (or browse to `192.168.4.1`): Wi-Fi credentials, robot name, personality prompt, volume, timezone, city for weather, and your AI API settings. After that, settings live at **http://frankenchan.local**.

Timezone uses POSIX format, e.g. `CET-1CEST,M3.5.0,M10.5.0/3` for central Europe or `EST5EDT,M3.2.0,M11.1.0` for US eastern.

## Controls

| Do this | It does this |
|---|---|
| Tap back touch panel | Start voice chat (tap again to stop listening) |
| Swipe back panel | Cycle Face / Clock / Weather / Timer / Network |
| Poke left cheek (screen) | Happy giggle + head tilt |
| Poke right cheek | Excited chirp + wink + nod |
| Tap forehead in Timer mode | +1 minute |
| Hold screen in Timer mode | Cancel timer |
| Tap an NFC tag to its face | Run that tag's action |
| Pick it up | Pauses idle motion, runs your pick-up action |
| Shake it | Runs your shake action |
| Tap the desk | Runs your desk-tap action |
| Walk up to it | Wakes, brightens, greets you |

## Project layout

```
src/main.cpp        glue: boot, Wi-Fi, input routing
src/fc_mood.*       mood engine: expressions, LEDs, chirps, idle quirks
src/fc_motion.*     idle motion task + gesture library
src/fc_voice.*      mic → STT → LLM → TTS → speaker, with lip-sync
src/fc_apps.*       glance modes: clock, weather (Open-Meteo), timer, network
src/fc_netmon.*     network monitor: RSSI, internet ping, AP scan, device presence
src/fc_face.*       face customizer: styles, color themes, battery icon, winks
src/fc_action.*     the shared action language (chat, say:, ir:, timer:, mood:, ...)
src/fc_ir.*         universal IR remote: learn + replay
src/fc_nfc.*        NFC tag -> action routines
src/fc_imu.*        pick-up / shake / desk-tap gestures
src/fc_sense.*      proximity + ambient light + camera motion presence
src/fc_espnow.*     ESP-NOW sensor mesh receiver
src/fc_mqtt.*       MQTT bridge + Home Assistant discovery
extras/sensor_node/ universal sensor-node firmware (own PlatformIO project, 3 chips)
flasher/            one web flasher page covering every firmware + manifests
src/fc_web.*        captive setup portal + settings + REST API + /dash dashboard
src/fc_config.*     NVS-backed settings
lib/StackChan-BSP/  vendored M5Stack board support (servos, touch, LEDs, battery)
```

## The action language

Everything that can happen is a short string, and every input can trigger any of them — NFC tags, gestures, MQTT, ESP-NOW alerts, the REST API, and the dashboard all speak it:

```
chat            say:Hello there    mood:happy      face:doggy     theme:mint
wink            ir:tv_power        timer:25        timer:cancel   mode:network
scan            nod                shake           tilt           wiggle
```

So `ir:desk_lamp` as your desk-tap gesture means knocking on your desk turns the lamp on.

## Build & flash notes

Pinned deliberately, all verified to exist and to be compatible with Arduino-ESP32 core 3.x (IDF 5.4):

| Piece | Version | Why |
|---|---|---|
| pioarduino platform | `54.03.20` | Arduino-ESP32 core 3.2.0 |
| IRremoteESP8266 | `^2.9.0` | 2.9.0 is the first release with core 3.x support |
| M5UnitUnified / M5Unit-NFC | `^0.5.0` / `^0.1.0` | NFC 0.1.0 requires UnitUnified >= 0.5.0 |
| ICMP ping | *(none)* | Uses the ESP-IDF native `esp_ping` stack instead of an abandoned library |
| Partition table | `default_16MB.csv` | 6.25 MB app slot; this firmware needs roughly 2 MB |

Merged flash images are built so that **every** target flashes at offset `0x0` — the classic ESP32's bootloader sits at `0x1000` and esptool pads the gap, while ESP32-S3/C3 start at `0x0`. CI verifies each manifest points at a binary that actually exists before publishing.

## Checked against the factory firmware

M5Stack publishes the StackChan factory firmware, so this firmware was audited against it. Two things it does that the Arduino BSP does not, both now matched here:

**Servo travel limits.** The factory clamps pitch to **30..870** and enables servo stall protection. The Arduino BSP allows the full **0..900** and has no stall-protection option at all — its `ServoConfig_t` doesn't expose the field. Since the physical end stops sit just outside the factory band, driving to 0 or 900 parks the servo against a stop and stalls it, which is how you cook a servo. Every motion command here is now clamped to the factory envelope, including `lookAtNormalized()`, which otherwise maps `y = -1.0` straight onto pitch 0. The REST endpoint `/api/move` inherits the same clamp.

**Idle torque.** The factory calls `setAutoTorqueReleaseEnabled(true)` so the servos de-energise when they aren't moving. Without it they hold torque continuously — running hot, buzzing audibly, and draining the battery while standing still. Now enabled at startup.

Also confirmed identical to the factory: camera data pins (D0-D7 = 39,40,41,42,15,16,48,47; VSYNC 46, HREF 38, PCLK 45), the 12-LED RGB ring, servo IDs (yaw = 1, pitch = 2) with zero positions 460 / 620, and yaw travel of +/-1280. The factory drives its camera through ESP-IDF's V4L2 stack, which Arduino doesn't have, so the camera here follows M5's own Arduino library instead.

## Notes on the optional hardware

**Camera** is compiled in via `-DFC_ENABLE_CAMERA=1` and disabled at runtime by default. It shares the internal I2C bus with the servos and touch panel *during init only*, so the firmware hands the bus over and takes it back. If you hit odd servo behaviour, untick the camera box or drop the build flag. No images ever leave the device — it only compares coarse brightness grids.

**Proximity** uses the CoreS3's LTR-553 over I2C. The raw threshold varies by unit; tune it on the setup page if it greets you from across the room or never notices you.

**ESP-NOW** peers must share a Wi-Fi channel. Nodes handle this by sweeping every channel when they report, so no setup is needed. If you know your router's channel you can pin it in the node's portal — it then transmits once instead of sweeping, which meaningfully extends battery life.

## Customizing

The fun knobs are all near the top of files: mood colors and idle-quirk odds in `fc_mood.cpp`, gesture choreography in `fc_motion.cpp`, the personality prompt lives in settings (no reflash needed), and glance modes are easy to extend in `fc_apps.cpp` — add an enum entry and a case.

## Credits & license

Built on [StackChan-BSP](https://github.com/m5stack/StackChan-BSP) (M5Stack, MIT), [M5Unified](https://github.com/m5stack/M5Unified), and [m5stack-avatar](https://github.com/meganetaaan/m5stack-avatar) by Shinya Ishikawa. StackChan was created by Shinya Ishikawa ([@stack_chan](https://x.com/stack_chan)). This firmware: MIT.
