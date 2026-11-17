// Daily routine assistant: a checklist of everyday jobs grouped by part of the
// day, stars for each finished job, gentle reminders at each job's time and an
// automatic reset once per day (at the configured hour, default 3:00 AM).
//
// The "routine day" is stored with the progress, so rebooting or re-checking
// never wipes today's ticks - only a new day does.
#pragma once
#include <Arduino.h>
#include "../config/Config.h"
#include "../core/TimeLogic.h"

struct RoutineTask {
  char name[KC_TASK_NAME_LEN];
  uint8_t period;    // tl::DayPeriod
  uint8_t enabled;
  uint8_t remind;    // pop-up reminder at `minute`
  uint8_t reserved;
  int16_t minute;    // minute of day for the reminder / sort order, -1 = none
  int16_t reserved2;
};

class RoutineManager {
 public:
  void begin();
  void loop(bool allowReminders);

  int count() const { return KC_MAX_TASKS; }
  RoutineTask& task(int i) { return cfg_.tasks[i]; }
  bool used(int i) const { return cfg_.tasks[i].enabled && cfg_.tasks[i].name[0]; }
  bool done(int i) const { return (st_.doneMask >> i) & 1; }
  void saveConfig();
  void resetDefaults();

  // Returns stars earned (0 if already done). Sets *allDone when the whole day is complete.
  int complete(int i, bool* allDone = nullptr);
  void undo(int i);
  void resetStars();

  int starsToday() const { return st_.starsToday; }
  uint32_t starsTotal() const { return st_.totalStars; }
  void progress(tl::DayPeriod p, int& done, int& total) const;
  void progressAll(int& done, int& total) const;

  // Indices of used tasks in one period, sorted by time.
  int tasksIn(tl::DayPeriod p, int* out, int max) const;

  // Reminder events: index of a task that is due now, or -1.
  int takeReminder();
  void snoozeReminder(int i, int minutes);

  void printStatus(Print& out) const;

 private:
  struct Config {
    RoutineTask tasks[KC_MAX_TASKS];
  };
  struct State {
    int32_t day;           // tl::routineDay of the stored progress
    uint32_t doneMask;
    uint16_t starsToday;
    uint8_t bonusGiven;
    uint8_t reserved;
    uint32_t totalStars;
    uint32_t remindedMask;
  };
  void saveState();
  Config cfg_{};
  State st_{};
  int pendingReminder_ = -1;
  int lastCheckedMinute_ = -1;
  time_t snoozeUntil_[KC_MAX_TASKS] = {0};
};

extern RoutineManager Routine;
