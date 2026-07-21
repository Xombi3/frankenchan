# ESP-NOW sensor node

Turn any spare ESP32 into a battery sensor that reports **directly** to FrankenChan — no router, no broker, no cloud, and no code editing.

## Flash it

Use the web flasher and pick your chip (ESP32 / ESP32-S3 / ESP32-C3). One binary works for every node — nothing is hard-coded.

## Set it up

1. Power it on — it opens a Wi-Fi network **`FC-Node-xxxx`** (password `stackchan`)
2. Join it; the setup page appears
3. Name the node, pick what it senses, set its **direction**, choose the GPIO pin, and optionally type what it should say
4. Save. It restarts and starts reporting.

To change settings later, the portal reopens for 30 seconds on every boot.

## Why there's no MAC address to copy

Nodes **broadcast**, so they never need to know StackChan's address, and they sweep Wi-Fi channels when reporting so they don't need to know your router's channel either. Flash, configure, done.

If you know your router's channel you can pin it in the portal — the node then transmits once instead of sweeping, which meaningfully extends battery life.

## Direction (bearing)

The most fun setting. Each node has a bearing in degrees (-120 to 120, 0 = straight ahead). When that sensor fires, StackChan physically **turns its head that way**, pulls a curious face, and reads your alert text out loud. Put the door sensor at -60 and the mailbox at +90 and the robot starts looking at things that happen in your house.

## Wiring

The default sketch reads a digital pin (reed switch, PIR, button — anything that pulls a pin low). Wire the sensor between your chosen GPIO and GND, leave "triggers when LOW" ticked, and it works.

For analog or I2C sensors (temperature, soil moisture), edit `readSensor()` in `src/main.cpp` and rebuild with `pio run -e node-esp32`.

## Battery life

Nodes stay awake by default so they react instantly. For long battery life on slow-changing sensors, pin the Wi-Fi channel and swap the `loop()` for a deep-sleep cycle — see the comment at the bottom of `src/main.cpp`.
