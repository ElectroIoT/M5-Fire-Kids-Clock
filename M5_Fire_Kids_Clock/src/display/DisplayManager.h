// Owns the full-screen 320x240 RGB565 canvas (in PSRAM). Screens draw into the
// canvas, then one push sends it to the ILI9342C - no flicker, no tearing of
// half-drawn frames. Redraws are rate-limited by the UI (see UiManager).
#pragma once
#include <M5Unified.h>

class DisplayManager {
 public:
  bool begin();
  LGFX_Sprite& canvas() { return canvas_; }
  void push();

  // level 1..10 (perceptual curve). 0 = backlight off.
  void setBrightness(uint8_t level);
  uint8_t brightness() const { return level_; }

  // Raw dump of the last pushed frame over serial (for remote screenshots).
  void dumpScreenshot(Stream& out);

  uint32_t lastPushMs() const { return lastPushUs_ / 1000; }
  uint32_t lastPushUs() const { return lastPushUs_; }
  uint32_t frames() const { return frames_; }

 private:
  LGFX_Sprite canvas_{&M5.Display};
  uint8_t level_ = 0;
  uint32_t lastPushUs_ = 0;
  uint32_t frames_ = 0;
};

extern DisplayManager Gfx;
