// The M5GO base's 10 SK6812 RGB LEDs (GPIO15). Non-blocking effects at ~33 fps.
// A persistent "base" effect (night glow, alarm flash, sunrise...) plus short
// one-shot overlays (task done, reward, notification) that fall back to the base.
#pragma once
#include <Arduino.h>

class LEDManager {
 public:
  enum class Effect : uint8_t { Off, Standby, Sunrise, Morning, Alarm, Notify, TaskDone, Reward, Night };

  void begin();
  void loop();
  void setBase(Effect e);
  Effect base() const { return base_; }
  void flash(Effect e, uint32_t ms);   // one-shot overlay
  void setNightCap(bool night) { nightCap_ = night; }
  static const char* name(Effect e);

 private:
  void render(Effect e, uint32_t t, uint32_t sinceStart);
  void set(int i, uint8_t r, uint8_t g, uint8_t b);
  Effect base_ = Effect::Off;
  Effect overlay_ = Effect::Off;
  uint32_t overlayUntil_ = 0, overlayStart_ = 0, baseStart_ = 0, lastMs_ = 0;
  bool nightCap_ = false;
  bool wasDark_ = false;
  uint8_t scale_ = 0;
};

extern LEDManager Leds;
