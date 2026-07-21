#include "fc_sense.h"
#include "fc_config.h"
#include "fc_mood.h"
#include <M5Unified.h>

#if FC_ENABLE_CAMERA
#include "esp_camera.h"
#endif

/* ------------------------- LTR-553 proximity / ALS ----------------------- */
// Registers per the M5CoreS3 LTR5XX driver.
static const uint8_t LTR_ADDR         = 0x23;
static const uint8_t LTR_ALS_CONTR    = 0x80;
static const uint8_t LTR_PS_CONTR     = 0x81;
static const uint8_t LTR_PS_DATA_LOW  = 0x8B;   // 0x8B/0x8C, high byte masked to 3 bits
static const uint8_t LTR_ALS_DATA_CH1 = 0x88;   // 0x88/0x89, relative brightness

static bool ltrOk = false;
static uint16_t psVal = 0, alsVal = 0;
static bool near_ = false;
static uint32_t lastPresence = 0;
static bool dimmed = false;

static bool ltrWrite(uint8_t reg, uint8_t val) {
    return M5.In_I2C.writeRegister(LTR_ADDR, reg, &val, 1, 400000);
}
static bool ltrRead(uint8_t reg, uint8_t* buf, size_t len) {
    return M5.In_I2C.readRegister(LTR_ADDR, reg, buf, len, 400000);
}

static void ltrInit() {
    // PS active mode, ALS active mode with default gain
    ltrOk = ltrWrite(LTR_PS_CONTR, 0x03);
    ltrOk = ltrWrite(LTR_ALS_CONTR, 0x01) && ltrOk;
    delay(20);
}

static void ltrPoll() {
    if (!ltrOk) return;
    uint8_t b[2];
    if (ltrRead(LTR_PS_DATA_LOW, b, 2)) {
        psVal = ((uint16_t)(b[1] & 0x07) << 8) | b[0];
    }
    if (ltrRead(LTR_ALS_DATA_CH1, b, 2)) {
        alsVal = ((uint16_t)b[1] << 8) | b[0];
    }
}

/* ---------------------------- camera motion ----------------------------- */

#if FC_ENABLE_CAMERA
static bool camOk = false;
static const int GX = 16, GY = 12;          // coarse motion grid
static uint8_t prevGrid[GX * GY];
static bool havePrev = false;
static bool motion = false;
static uint32_t lastMotionAt = 0;

static bool camInit() {
    camera_config_t c = {};
    c.pin_pwdn = -1; c.pin_reset = -1; c.pin_xclk = -1;
    c.pin_sccb_sda = 12; c.pin_sccb_scl = 11;
    c.pin_d7 = 47; c.pin_d6 = 48; c.pin_d5 = 16; c.pin_d4 = 15;
    c.pin_d3 = 42; c.pin_d2 = 41; c.pin_d1 = 40; c.pin_d0 = 39;
    c.pin_vsync = 46; c.pin_href = 38; c.pin_pclk = 45;
    c.xclk_freq_hz = 20000000;
    c.ledc_timer = LEDC_TIMER_0;
    c.ledc_channel = LEDC_CHANNEL_0;
    c.pixel_format = PIXFORMAT_GRAYSCALE;    // cheapest for frame differencing
    c.frame_size = FRAMESIZE_QQVGA;          // 160x120
    c.jpeg_quality = 0;
    c.fb_count = 1;
    c.fb_location = CAMERA_FB_IN_PSRAM;
    c.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    c.sccb_i2c_port = -1;

    // The sensor is configured over the shared internal I2C bus; hand it over,
    // then take the bus back for the servos / touch / IO expander.
    M5.In_I2C.release();
    esp_err_t err = esp_camera_init(&c);
    M5.In_I2C.begin(I2C_NUM_0, 12, 11);
    return err == ESP_OK;
}

static void camPoll() {
    if (!camOk) return;
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return;
    if (fb->format != PIXFORMAT_GRAYSCALE || fb->width < GX || fb->height < GY) {
        esp_camera_fb_return(fb);
        return;
    }
    // average each grid cell
    uint8_t grid[GX * GY];
    int cw = fb->width / GX, ch = fb->height / GY;
    for (int gy = 0; gy < GY; gy++) {
        for (int gx = 0; gx < GX; gx++) {
            uint32_t sum = 0;
            for (int y = 0; y < ch; y += 2) {
                const uint8_t* row = fb->buf + (size_t)(gy * ch + y) * fb->width + gx * cw;
                for (int x = 0; x < cw; x += 2) sum += row[x];
            }
            int n = ((ch + 1) / 2) * ((cw + 1) / 2);
            grid[gy * GX + gx] = n ? (uint8_t)(sum / n) : 0;
        }
    }
    esp_camera_fb_return(fb);

    if (havePrev) {
        int changed = 0;
        for (int i = 0; i < GX * GY; i++) {
            if (abs((int)grid[i] - (int)prevGrid[i]) > cfg.motion_sensitivity) changed++;
        }
        // a few cells changing = someone moving; a huge change = lighting shift
        if (changed >= 4 && changed < (GX * GY) / 2) {
            motion = true;
            lastMotionAt = millis();
        } else if (millis() - lastMotionAt > 20000) {
            motion = false;
        }
    }
    memcpy(prevGrid, grid, sizeof(grid));
    havePrev = true;
}
#endif  // FC_ENABLE_CAMERA

/* --------------------------------- api ---------------------------------- */

void senseInit() {
    ltrInit();
    lastPresence = millis();
#if FC_ENABLE_CAMERA
    if (cfg.camera_on) camOk = camInit();
#endif
}

void senseUpdate() {
    static uint32_t nextProx = 0, nextCam = 0;
    uint32_t now = millis();

    if (now >= nextProx) {
        nextProx = now + 300;
        ltrPoll();
        bool wasNear = near_;
        near_ = ltrOk && psVal > cfg.prox_threshold;
        if (near_) lastPresence = now;
        if (near_ && !wasNear) {
            // someone approached: wake up
            if (dimmed) { M5.Display.setBrightness(cfg.brightness); dimmed = false; }
            if (cfg.prox_greet) { moodSet(Mood::Happy, 3000); moodChirp(Mood::Happy); }
        }
    }

#if FC_ENABLE_CAMERA
    if (cfg.camera_on && now >= nextCam) {
        nextCam = now + 1500;
        camPoll();
        if (motion) lastPresence = now;
    }
#endif

    // auto-dim when nobody has been around for a while
    if (cfg.dim_after_s > 0) {
        bool away = (now - lastPresence) > (uint32_t)cfg.dim_after_s * 1000UL;
        if (away && !dimmed) { M5.Display.setBrightness(8); dimmed = true; }
        else if (!away && dimmed) { M5.Display.setBrightness(cfg.brightness); dimmed = false; }
    }
}

bool     senseNear()        { return near_; }
uint16_t senseProximity()   { return psVal; }
uint16_t senseLight()       { return alsVal; }
uint32_t senseLastPresenceMs() { return millis() - lastPresence; }

#if FC_ENABLE_CAMERA
bool senseMotion()      { return motion; }
bool senseCameraReady() { return camOk; }
#else
bool senseMotion()      { return false; }
bool senseCameraReady() { return false; }
#endif

String senseJson() {
    return String("{\"near\":") + (senseNear() ? "true" : "false") +
           ",\"proximity\":" + String(psVal) +
           ",\"light\":" + String(alsVal) +
           ",\"motion\":" + String(senseMotion() ? "true" : "false") +
           ",\"camera\":" + String(senseCameraReady() ? "true" : "false") +
           ",\"away_ms\":" + String(senseLastPresenceMs()) + "}";
}
