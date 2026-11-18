// Offline alarm engine: up to 10 alarms in NVS, minute-accurate scheduling from
// the ESP32 system clock (no Wi-Fi needed), snooze, auto-snooze, catch-up after
// small clock jumps and protection against ringing twice in the same minute.
#pragma once
#include <Arduino.h>
#include "../config/Config.h"
#include "../core/AlarmLogic.h"

class AlarmManager {
 public:
  void begin();
  void loop();

  al::AlarmConfig& alarm(int i) { return alarms_[i]; }
  int count() const { return KC_MAX_ALARMS; }
  void save();
  void resetDefaults();
  int enabledCount() const;

  // Ringing state
  bool ringing() const { return ringing_ >= 0; }
  int ringingIndex() const { return ringing_; }
  uint32_t ringingForMs() const { return ringing() ? millis() - ringStartMs_ : 0; }
  bool ringingIsSnooze() const { return ringingSnooze_; }
  void snooze();
  void stop();
  void test(int i);   // ring alarm i now (preview / serial test)

  // Events for the app layer: +1 started ringing, -1 stopped, 0 nothing.
  int takeEvent() { int e = event_; event_ = 0; return e; }

  // Next ring from now, including snoozes. Returns false if nothing scheduled.
  bool next(int& index, int& minutesAway, int& hour24, int& minute, bool& isSnooze) const;
  bool snoozed(int i) const { return snoozeUntil_[i] != 0; }

  void printStatus(Print& out) const;

 private:
  void startRing(int i, bool snooze);
  void checkMinute(int64_t epochMinute);

  al::AlarmConfig alarms_[KC_MAX_ALARMS];
  int64_t firedMinute_[KC_MAX_ALARMS] = {0};
  time_t snoozeUntil_[KC_MAX_ALARMS] = {0};
  uint16_t pending_ = 0;     // alarms that fired while another was ringing
  int ringing_ = -1;
  bool ringingSnooze_ = false;
  uint32_t ringStartMs_ = 0;
  int autoSnoozes_ = 0;
  int64_t lastMinute_ = 0;
  uint32_t lastClockSteps_ = 0;
  int event_ = 0;
  int resumeIdx_ = -1;        // alarm to ring again after an unexpected restart
  uint32_t resumeEpoch_ = 0;
};

extern AlarmManager Alarms;
