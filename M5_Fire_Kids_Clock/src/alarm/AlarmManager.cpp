#include "AlarmManager.h"
#include "../core/TimeLogic.h"
#include "../clock/ClockManager.h"
#include "../settings/SettingsManager.h"
#include "../storage/StorageManager.h"
#include "../audio/SoundList.h"

AlarmManager Alarms;

static constexpr uint16_t kAlarmVersion = 1;

// Survives watchdog / software resets: the last minute the engine processed,
// so a crash-reboot neither loses nor repeats an alarm.
RTC_NOINIT_ATTR static int64_t g_lastMinute;
RTC_NOINIT_ATTR static uint32_t g_lastMinuteMagic;
static constexpr uint32_t kMinuteMagic = 0xA1A2A3A4;

static void preset(al::AlarmConfig& a, int h, int m, uint8_t rep, uint8_t sound, const char* label) {
  memset(&a, 0, sizeof a);
  a.enabled = 0;  // presets start OFF - a new clock never rings by surprise
  a.hour = h;
  a.minute = m;
  a.repeat = rep;
  a.days = al::DAYS_WEEKDAYS;
  a.sound = sound;
  strlcpy(a.label, label, sizeof a.label);
}

void AlarmManager::resetDefaults() {
  preset(alarms_[0], 6, 30, al::REPEAT_WEEKDAYS, sounds::GENTLE_WAKE, "School Wake Up");
  preset(alarms_[1], 17, 0, al::REPEAT_WEEKDAYS, sounds::TWINKLE, "Homework Time");
  preset(alarms_[2], 21, 0, al::REPEAT_DAILY, sounds::MORNING_SONG, "Bedtime");
  for (int i = 3; i < KC_MAX_ALARMS; ++i) {
    char l[16];
    snprintf(l, sizeof l, "Alarm %d", i + 1);
    preset(alarms_[i], 7, 0, al::REPEAT_ONCE, sounds::CARTOON_ALARM, l);
  }
}

void AlarmManager::begin() {
  resetDefaults();
  auto r = Storage.loadBlob("alarms", kAlarmVersion, alarms_, sizeof alarms_);
  if (r == StorageManager::LoadResult::Corrupt) resetDefaults();
  bool fixed = false;
  for (auto& a : alarms_) fixed |= al::sanitize(a);
  if (fixed || r == StorageManager::LoadResult::Migrated) save();
  KC_LOG("Alarms: %s, %d enabled", StorageManager::resultName(r), enabledCount());

  const esp_reset_reason_t rr = esp_reset_reason();
  const bool warm = rr != ESP_RST_POWERON && rr != ESP_RST_BROWNOUT && rr != ESP_RST_UNKNOWN;
  if (warm && g_lastMinuteMagic == kMinuteMagic) {
    lastMinute_ = g_lastMinute;  // resume: catch-up logic decides what to replay
    KC_LOG("Alarms: resuming after reset");
  }
  g_lastMinuteMagic = kMinuteMagic;
  lastClockSteps_ = Clock.stepCount();
}

void AlarmManager::save() { Storage.saveBlob("alarms", kAlarmVersion, alarms_, sizeof alarms_); }

int AlarmManager::enabledCount() const {
  int n = 0;
  for (const auto& a : alarms_) n += a.enabled ? 1 : 0;
  return n;
}

void AlarmManager::startRing(int i, bool snooze) {
  if (ringing_ >= 0) {
    if (ringing_ != i) pending_ |= (1u << i);  // ring after the current one
    return;
  }
  ringing_ = i;
  ringingSnooze_ = snooze;
  ringStartMs_ = millis();
  event_ = 1;
  char hm[12];
  tl::formatHM12AmPm(hm, sizeof hm, alarms_[i].hour, alarms_[i].minute);
  KC_LOG("ALARM %d ringing: %s '%s'%s", i + 1, hm, alarms_[i].label, snooze ? " (snooze)" : "");
}

void AlarmManager::checkMinute(int64_t m) {
  const tl::LocalParts p = tl::localFromEpochMinute(m, tl::IST_OFFSET_MIN);
  for (int i = 0; i < KC_MAX_ALARMS; ++i) {
    al::AlarmConfig& a = alarms_[i];
    if (!a.enabled || firedMinute_[i] == m) continue;
    if (!al::matches(a, p.wday, p.hour, p.minute)) continue;
    firedMinute_[i] = m;
    snoozeUntil_[i] = 0;
    if (a.repeat == al::REPEAT_ONCE) {
      a.enabled = 0;  // one-time alarm: switch itself off
      save();
    }
    startRing(i, false);
  }
}

void AlarmManager::loop() {
  if (!Clock.isValid()) return;  // no reliable time -> never guess
  const time_t now = time(nullptr);
  const int64_t minute = (int64_t)now / 60;

  if (lastMinute_ == 0) {
    lastMinute_ = minute - 1;  // first run: check the current minute too
  }
  if (Clock.stepCount() != lastClockSteps_) {
    lastClockSteps_ = Clock.stepCount();
    // Clock was set / synced. Small forward steps are caught up below; a big
    // jump (or any backwards step) just re-anchors without replaying alarms.
    const int64_t d = minute - lastMinute_;
    if (d < 0 || d > KC_ALARM_CATCHUP_MIN) lastMinute_ = minute - 1;
  }
  if (minute > lastMinute_) {
    int64_t from = lastMinute_ + 1;
    if (minute - from > KC_ALARM_CATCHUP_MIN) from = minute - KC_ALARM_CATCHUP_MIN;
    for (int64_t m = from; m <= minute; ++m) checkMinute(m);
    lastMinute_ = minute;
    g_lastMinute = minute;
  } else if (minute < lastMinute_) {
    lastMinute_ = minute;  // clock went back; firedMinute_ prevents repeats
    g_lastMinute = minute;
  }

  // Snoozes
  for (int i = 0; i < KC_MAX_ALARMS; ++i) {
    if (snoozeUntil_[i] && now >= snoozeUntil_[i]) {
      snoozeUntil_[i] = 0;
      startRing(i, true);
    }
  }

  // Ringing too long without a response -> auto-snooze, then give up.
  if (ringing_ >= 0 && millis() - ringStartMs_ > KC_ALARM_RING_MAX_MS) {
    if (++autoSnoozes_ > KC_ALARM_MAX_AUTOSNOOZE) {
      KC_LOG("ALARM %d: no response, stopping", ringing_ + 1);
      stop();
    } else {
      KC_LOG("ALARM %d: no response, auto-snooze %d", ringing_ + 1, autoSnoozes_);
      const int keep = autoSnoozes_;
      snooze();
      autoSnoozes_ = keep;
    }
  }

  // Next queued alarm
  if (ringing_ < 0 && pending_) {
    for (int i = 0; i < KC_MAX_ALARMS; ++i) {
      if (pending_ & (1u << i)) {
        pending_ &= ~(1u << i);
        startRing(i, false);
        break;
      }
    }
  }
}

void AlarmManager::snooze() {
  if (ringing_ < 0) return;
  const int i = ringing_;
  snoozeUntil_[i] = time(nullptr) + AppCfg.get().snoozeMin * 60;
  KC_LOG("ALARM %d snoozed for %d min", i + 1, AppCfg.get().snoozeMin);
  ringing_ = -1;
  event_ = -1;
}

void AlarmManager::stop() {
  if (ringing_ < 0) return;
  KC_LOG("ALARM %d stopped", ringing_ + 1);
  snoozeUntil_[ringing_] = 0;
  ringing_ = -1;
  autoSnoozes_ = 0;
  event_ = -1;
}

void AlarmManager::test(int i) {
  if (i < 0 || i >= KC_MAX_ALARMS) return;
  startRing(i, false);
}

bool AlarmManager::next(int& index, int& minutesAway, int& hour24, int& minute, bool& isSnooze) const {
  if (!Clock.isValid()) return false;
  const time_t now = time(nullptr);
  const tl::LocalParts p = tl::localFromEpochMinute((int64_t)now / 60, tl::IST_OFFSET_MIN);
  int best = -1, bestMin = INT32_MAX;
  bool bestSnooze = false;
  for (int i = 0; i < KC_MAX_ALARMS; ++i) {
    const int m = al::minutesUntilNext(alarms_[i], p.wday, p.hour, p.minute);
    if (m > 0 && m < bestMin) { best = i; bestMin = m; bestSnooze = false; }
    if (snoozeUntil_[i]) {
      const int s = (int)((snoozeUntil_[i] - now + 59) / 60);
      if (s < bestMin) { best = i; bestMin = s < 0 ? 0 : s; bestSnooze = true; }
    }
  }
  if (best < 0) return false;
  index = best;
  minutesAway = bestMin;
  isSnooze = bestSnooze;
  if (bestSnooze) {
    const tl::LocalParts q = tl::localFromEpochMinute((int64_t)snoozeUntil_[best] / 60, tl::IST_OFFSET_MIN);
    hour24 = q.hour;
    minute = q.minute;
  } else {
    hour24 = alarms_[best].hour;
    minute = alarms_[best].minute;
  }
  return true;
}

void AlarmManager::printStatus(Print& out) const {
  for (int i = 0; i < KC_MAX_ALARMS; ++i) {
    const auto& a = alarms_[i];
    char hm[12], days[32];
    tl::formatHM12AmPm(hm, sizeof hm, a.hour, a.minute);
    al::describeDays(a, days, sizeof days);
    out.printf("  %2d %s %s %-10s %-16s sound=%d%s\n", i + 1, a.enabled ? "ON " : "off", hm, days, a.label, a.sound,
               snoozeUntil_[i] ? " (snoozed)" : "");
  }
  int idx, mins, h, m;
  bool sn;
  if (next(idx, mins, h, m, sn)) {
    char hm[12];
    tl::formatHM12AmPm(hm, sizeof hm, h, m);
    out.printf("  next: #%d at %s (in %d min)%s\n", idx + 1, hm, mins, sn ? " snooze" : "");
  }
  out.printf("  ringing: %d\n", ringing_ + 1);
}
