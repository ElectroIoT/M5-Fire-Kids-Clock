#include "NightMode.h"
#include "../config/Config.h"
#include "../core/TimeLogic.h"
#include "../settings/SettingsManager.h"

NightModeManager NightMode;

void NightModeManager::loop(const LocalTime& t) {
  const Settings& s = AppCfg.get();
  bool inWin = false;
  if (t.valid && s.nightEnabled) inWin = tl::inWindow(tl::minuteOfDay(t.hour, t.minute), s.nightStartMin, s.nightEndMin);

  // "Sleep now" lasts until the end of the next (or current) night window.
  if (forced_) {
    if (inWin) forcedSawWindow_ = true;
    else if (forcedSawWindow_) forced_ = false;
    // Without a usable window (night mode off / no time), end it at sunrise-ish.
    if (!s.nightEnabled && t.valid && t.hour >= 6 && t.hour < 12) forced_ = false;
  }

  scheduled_ = inWin || forced_;
  if (!scheduled_) skipped_ = false;  // a new window starts fresh
  if (scheduled_ != wasScheduled_) {
    edge_ = scheduled_ ? 1 : -1;
    KC_LOG("Night mode: %s", scheduled_ ? "start" : "end");
    wasScheduled_ = scheduled_;
  }
}

void NightModeManager::skipTonight() {
  if (forced_) {
    forced_ = false;
    return;
  }
  skipped_ = scheduled_;
}

void NightModeManager::forceNow(bool on) {
  forced_ = on;
  forcedSawWindow_ = false;
  skipped_ = false;
  peekUntil_ = millis();
}
