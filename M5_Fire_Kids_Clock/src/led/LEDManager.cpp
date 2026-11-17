#include "LEDManager.h"
#include <Adafruit_NeoPixel.h>
#include <math.h>
#include "../config/Config.h"
#include "../config/Pins.h"
#include "../settings/SettingsManager.h"

LEDManager Leds;

// SK6812 on the M5GO bar: GRB order, 800 kHz.
static Adafruit_NeoPixel strip(pins::RGB_LED_COUNT, pins::RGB_LED, NEO_GRB + NEO_KHZ800);

void LEDManager::begin() {
  strip.begin();
  strip.clear();
  strip.show();
  KC_LOG("LEDs: %d x SK6812 on GPIO%d", pins::RGB_LED_COUNT, pins::RGB_LED);
}

const char* LEDManager::name(Effect e) {
  switch (e) {
    case Effect::Off: return "off";
    case Effect::Standby: return "standby";
    case Effect::Sunrise: return "sunrise";
    case Effect::Morning: return "morning";
    case Effect::Alarm: return "alarm";
    case Effect::Notify: return "notify";
    case Effect::TaskDone: return "task";
    case Effect::Reward: return "reward";
    default: return "night";
  }
}

void LEDManager::setBase(Effect e) {
  if (e == base_) return;
  base_ = e;
  baseStart_ = millis();
}

void LEDManager::flash(Effect e, uint32_t ms) {
  overlay_ = e;
  overlayStart_ = millis();
  overlayUntil_ = overlayStart_ + ms;
}

void LEDManager::set(int i, uint8_t r, uint8_t g, uint8_t b) {
  strip.setPixelColor(i, (r * scale_) >> 8, (g * scale_) >> 8, (b * scale_) >> 8);
}

static void hsv(float h, uint8_t& r, uint8_t& g, uint8_t& b) {
  const uint32_t c = Adafruit_NeoPixel::ColorHSV((uint16_t)(h * 65535.f), 200, 255);
  r = (c >> 16) & 0xFF;
  g = (c >> 8) & 0xFF;
  b = c & 0xFF;
}

void LEDManager::render(Effect e, uint32_t t, uint32_t since) {
  const int n = pins::RGB_LED_COUNT;
  switch (e) {
    case Effect::Off:
      strip.clear();
      break;
    case Effect::Standby:  // a barely-there warm glow
      for (int i = 0; i < n; ++i) set(i, 12, 6, 2);
      break;
    case Effect::Sunrise: {  // red -> orange -> warm yellow over 10 minutes
      const float p = min(1.0f, since / 600000.0f);
      for (int i = 0; i < n; ++i) {
        const float w = 0.6f + 0.4f * sinf(t / 900.0f + i * 0.6f);
        set(i, (uint8_t)(255 * w), (uint8_t)((60 + 150 * p) * w), (uint8_t)(20 * p * w));
      }
      break;
    }
    case Effect::Morning:  // slow drifting pastels
      for (int i = 0; i < n; ++i) {
        uint8_t r, g, b;
        hsv(fmodf(t / 12000.0f + i / (float)n, 1.0f), r, g, b);
        set(i, (r + 255) / 2, (g + 255) / 2, (b + 255) / 2);
      }
      break;
    case Effect::Alarm: {  // pink / red flashing, alternating halves
      const bool ph = (t / 250) & 1;
      for (int i = 0; i < n; ++i) {
        const bool on = ((i < n / 2) ^ ph);
        if (on) set(i, 255, 40, 90);
        else set(i, 120, 0, 0);
      }
      break;
    }
    case Effect::Notify: {  // gentle blue breathing
      const float w = 0.5f - 0.5f * cosf(since / 300.0f);
      for (int i = 0; i < n; ++i) set(i, (uint8_t)(40 * w), (uint8_t)(120 * w), (uint8_t)(255 * w));
      break;
    }
    case Effect::TaskDone: {  // green wipe
      const int lit = min<int>(n, since / 60 + 1);
      for (int i = 0; i < n; ++i) {
        if (i < lit) set(i, 40, 230, 120);
        else strip.setPixelColor(i, 0);
      }
      break;
    }
    case Effect::Reward:  // spinning rainbow
      for (int i = 0; i < n; ++i) {
        uint8_t r, g, b;
        hsv(fmodf(t / 800.0f + i / (float)n, 1.0f), r, g, b);
        set(i, r, g, b);
      }
      break;
    case Effect::Night:  // dim warm purple night light
      for (int i = 0; i < n; ++i) set(i, 40, 8, 50);
      break;
  }
}

void LEDManager::loop() {
  const uint32_t t = millis();
  if (t - lastMs_ < 30) return;
  lastMs_ = t;
  const Settings& s = AppCfg.get();

  Effect e = base_;
  uint32_t start = baseStart_;
  if (overlay_ != Effect::Off && (int32_t)(overlayUntil_ - t) > 0) {
    e = overlay_;
    start = overlayStart_;
  } else {
    overlay_ = Effect::Off;
  }
  // Global brightness: setting 1..10, much lower at night. Alarms may be bright.
  uint16_t sc = 14 + s.ledBrightness * 24;  // 38..254
  if (nightCap_ && e != Effect::Alarm) sc = min<uint16_t>(sc, 40);
  if (!s.ledEnabled) e = Effect::Off;
  scale_ = (uint8_t)min<uint16_t>(sc, 255);

  const bool dark = e == Effect::Off;
  if (dark && wasDark_) return;  // nothing to refresh
  wasDark_ = dark;
  render(e, t, t - start);
  strip.show();
}
