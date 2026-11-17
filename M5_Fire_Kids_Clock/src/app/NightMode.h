// Automatic night mode: dark, very dim, minimal clock between the configured
// start and end times. It never blocks alarms. A button press "peeks" (normal
// screen for 30 s); a long press on C skips night mode until the next window.
#pragma once
#include <Arduino.h>
#include "../clock/ClockManager.h"

class NightModeManager {
 public:
  void loop(const LocalTime& t);
  bool scheduled() const { return scheduled_; }       // inside the window (and enabled)
  bool active() const { return scheduled_ && !skipped_ && !peeking(); }
  bool peeking() const { return (int32_t)(peekUntil_ - millis()) > 0; }
  void peek(uint32_t ms = 30000) { peekUntil_ = millis() + ms; }
  void endPeek() { peekUntil_ = millis(); }
  void skipTonight();
  bool skipped() const { return skipped_; }
  void forceNow(bool on);      // "Sleep now" from the menu / serial
  // +1 when night mode just started, -1 when it just ended, 0 otherwise.
  int takeEdge() { int e = edge_; edge_ = 0; return e; }

 private:
  bool scheduled_ = false;
  bool skipped_ = false;
  bool forced_ = false;
  bool forcedSawWindow_ = false;
  bool wasScheduled_ = false;
  uint32_t peekUntil_ = 0;
  int edge_ = 0;
};

extern NightModeManager NightMode;
