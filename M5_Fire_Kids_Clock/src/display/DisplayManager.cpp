#include "DisplayManager.h"
#include "../config/Config.h"

DisplayManager Gfx;

bool DisplayManager::begin() {
  // M5Unified already picks the upright landscape rotation for the Fire
  // (rotation numbering differs between M5GFX versions, so never hard-code 1).
  KC_LOG("Display: default rotation %d, %dx%d", M5.Display.getRotation(), (int)M5.Display.width(), (int)M5.Display.height());
  if (M5.Display.width() < M5.Display.height()) M5.Display.setRotation((M5.Display.getRotation() + 1) & 3);
  canvas_.setPsram(true);
  canvas_.setColorDepth(16);
  if (!canvas_.createSprite(KC_SCREEN_W, KC_SCREEN_H)) {
    // Fallback: internal RAM (only if PSRAM is missing / disabled in the build).
    canvas_.setPsram(false);
    if (!canvas_.createSprite(KC_SCREEN_W, KC_SCREEN_H)) {
      KC_LOG("Display: canvas allocation FAILED");
      return false;
    }
    KC_LOG("Display: canvas in internal RAM (PSRAM not available!)");
  }
  canvas_.fillScreen((uint32_t)0xFFF8EEu);
  return true;
}

void DisplayManager::push() {
  const uint32_t t0 = micros();
  canvas_.pushSprite(0, 0);
  lastPushUs_ = micros() - t0;
  ++frames_;
}

void DisplayManager::setBrightness(uint8_t level) {
  // Perceptual curve; level 1 is a faint night glow.
  static const uint8_t curve[11] = {0, 6, 16, 30, 48, 70, 96, 126, 160, 200, 245};
  if (level > 10) level = 10;
  if (level == level_) return;
  level_ = level;
  M5.Display.setBrightness(curve[level]);
}

void DisplayManager::dumpScreenshot(Stream& out) {
  const uint16_t* buf = static_cast<const uint16_t*>(canvas_.getBuffer());
  if (!buf) {
    out.println("SHOT ERR");
    return;
  }
  // Header line, then W*H big-endian RGB565 words (sprite memory is already swapped).
  out.printf("SHOT %d %d\n", KC_SCREEN_W, KC_SCREEN_H);
  const uint8_t* p = reinterpret_cast<const uint8_t*>(buf);
  const size_t total = KC_SCREEN_W * KC_SCREEN_H * 2;
  for (size_t off = 0; off < total; off += 1024) {
    out.write(p + off, min<size_t>(1024, total - off));
  }
  out.println();
  out.println("SHOT END");
}
