#include "RoutineManager.h"
#include "../clock/ClockManager.h"
#include "../settings/SettingsManager.h"
#include "../storage/StorageManager.h"
#include "../api/EventLog.h"

RoutineManager Routine;

static constexpr uint16_t kCfgVersion = 1;
static constexpr uint16_t kStateVersion = 1;
static constexpr int kAllDoneBonus = 3;

void RoutineManager::resetDefaults() {
  memset(&cfg_, 0, sizeof cfg_);
  struct D { const char* n; uint8_t p; int h, m; uint8_t remind; };
  static const D d[] = {
      {"Wake up", tl::PERIOD_MORNING, 6, 30, 0},
      {"Brush teeth", tl::PERIOD_MORNING, 6, 40, 1},
      {"Take a bath", tl::PERIOD_MORNING, 6, 50, 1},
      {"Get dressed", tl::PERIOD_MORNING, 7, 5, 1},
      {"Have breakfast", tl::PERIOD_MORNING, 7, 15, 1},
      {"Pack school bag", tl::PERIOD_MORNING, 7, 30, 1},
      {"Back from school", tl::PERIOD_AFTERNOON, 13, 30, 0},
      {"Have lunch", tl::PERIOD_AFTERNOON, 14, 0, 1},
      {"Rest time", tl::PERIOD_AFTERNOON, 14, 30, 1},
      {"Homework", tl::PERIOD_EVENING, 17, 0, 1},
      {"Playtime", tl::PERIOD_EVENING, 18, 0, 0},
      {"Read a book", tl::PERIOD_EVENING, 19, 30, 1},
      {"Brush teeth", tl::PERIOD_NIGHT, 20, 30, 1},
      {"Get ready for bed", tl::PERIOD_NIGHT, 20, 45, 1},
      {"Sleep", tl::PERIOD_NIGHT, 20, 55, 1},
  };
  int i = 0;
  for (const auto& x : d) {
    RoutineTask& t = cfg_.tasks[i++];
    strlcpy(t.name, x.n, sizeof t.name);
    t.period = x.p;
    t.enabled = 1;
    t.remind = x.remind;
    t.minute = (int16_t)(x.h * 60 + x.m);
  }
  for (; i < KC_MAX_TASKS; ++i) cfg_.tasks[i].minute = -1;  // spare slots for parents
}

void RoutineManager::begin() {
  resetDefaults();
  auto r = Storage.loadBlob("routine", kCfgVersion, &cfg_, sizeof cfg_);
  if (r == StorageManager::LoadResult::Corrupt) resetDefaults();
  for (auto& t : cfg_.tasks) {
    t.name[sizeof t.name - 1] = '\0';
    if (t.period >= tl::PERIOD_COUNT) t.period = tl::PERIOD_MORNING;
    if (t.minute < -1 || t.minute >= 1440) t.minute = -1;
    t.enabled = t.enabled ? 1 : 0;
    t.remind = t.remind ? 1 : 0;
  }
  memset(&st_, 0, sizeof st_);
  auto s = Storage.loadBlob("rstate", kStateVersion, &st_, sizeof st_);
  if (s != StorageManager::LoadResult::Ok && s != StorageManager::LoadResult::Migrated) memset(&st_, 0, sizeof st_);
  KC_LOG("Routine: tasks %s, progress %s (day %ld, %u stars today, %lu total)", StorageManager::resultName(r),
         StorageManager::resultName(s), (long)st_.day, st_.starsToday, (unsigned long)st_.totalStars);
}

void RoutineManager::saveConfig() { Storage.saveBlob("routine", kCfgVersion, &cfg_, sizeof cfg_); }
void RoutineManager::saveState() { Storage.saveBlob("rstate", kStateVersion, &st_, sizeof st_); }

void RoutineManager::loop(bool allowReminders) {
  if (!Clock.isValid()) return;
  const time_t now = time(nullptr);
  const int32_t day = tl::routineDay(now, tl::IST_OFFSET_MIN, AppCfg.get().routineResetHour);
  // A clock corrected backwards by up to a day (NTP after a wrong manual set)
  // must not wipe today's ticks; any other change of day is a new day.
  if (day == st_.day - 1 && st_.day != 0) {
    st_.day = day;  // re-anchor, keep the ticks; the next real day still resets
    saveState();
  } else if (day != st_.day) {
    // A new routine day: clear the ticks (stars total is kept). Happens once,
    // because the day number is saved with the progress.
    KC_LOG("Routine: new day (%ld -> %ld), checklist reset", (long)st_.day, (long)day);
    st_.day = day;
    st_.doneMask = 0;
    st_.starsToday = 0;
    st_.bonusGiven = 0;
    st_.remindedMask = 0;
    memset(snoozeUntil_, 0, sizeof snoozeUntil_);
    saveState();
  }

  // Reminders: checked once per minute, never during night mode / alarms.
  const LocalTime t = Clock.now();
  const int mod = t.hour * 60 + t.minute;
  const bool newMinute = mod != lastCheckedMinute_;
  lastCheckedMinute_ = mod;
  if (!AppCfg.get().routineReminders || !allowReminders || pendingReminder_ >= 0) return;
  for (int i = 0; i < KC_MAX_TASKS; ++i) {
    const RoutineTask& k = cfg_.tasks[i];
    if (!used(i) || !k.remind || done(i)) continue;
    const bool snoozeDue = snoozeUntil_[i] && now >= snoozeUntil_[i];
    const bool timeDue = newMinute && k.minute == mod && !((st_.remindedMask >> i) & 1);
    if (snoozeDue || timeDue) {
      snoozeUntil_[i] = 0;
      st_.remindedMask |= (1u << i);
      pendingReminder_ = i;
      KC_LOG("Routine: reminder '%s'", k.name);
      break;
    }
  }
}

int RoutineManager::takeReminder() {
  const int r = pendingReminder_;
  pendingReminder_ = -1;
  return r;
}

void RoutineManager::snoozeReminder(int i, int minutes) {
  if (i >= 0 && i < KC_MAX_TASKS) snoozeUntil_[i] = time(nullptr) + minutes * 60;
}

int RoutineManager::complete(int i, bool* allDone) {
  if (allDone) *allDone = false;
  if (i < 0 || i >= KC_MAX_TASKS || done(i)) return 0;
  st_.doneMask |= (1u << i);
  snoozeUntil_[i] = 0;
  int earned = 1;
  int d, total;
  progressAll(d, total);
  if (d == total && total > 0 && !st_.bonusGiven) {
    st_.bonusGiven = 1;
    earned += kAllDoneBonus;
    if (allDone) *allDone = true;
  }
  st_.starsToday += earned;
  st_.totalStars += earned;
  saveState();
  Events.add(EventType::RoutineDone, i, cfg_.tasks[i].name);
  int pd, pt;
  progress((tl::DayPeriod)cfg_.tasks[i].period, pd, pt);
  if (pt > 0 && pd == pt) Events.add(EventType::RoutinePeriodDone, -1, tl::periodName((tl::DayPeriod)cfg_.tasks[i].period));
  if (earned > 1) Events.add(EventType::RoutineAllDone, -1, "all jobs done today");  // bonus = whole day done
  KC_LOG("Routine: '%s' done, +%d star(s)", cfg_.tasks[i].name, earned);
  return earned;
}

void RoutineManager::undo(int i) {
  if (i < 0 || i >= KC_MAX_TASKS || !done(i)) return;
  st_.doneMask &= ~(1u << i);
  int take = 1;
  if (st_.bonusGiven) {  // the day is no longer complete
    st_.bonusGiven = 0;
    take += kAllDoneBonus;
  }
  st_.starsToday = st_.starsToday >= take ? st_.starsToday - take : 0;
  st_.totalStars = st_.totalStars >= (uint32_t)take ? st_.totalStars - take : 0;
  saveState();
}

void RoutineManager::resetStars() {
  st_.starsToday = 0;
  st_.totalStars = 0;
  saveState();
}

void RoutineManager::progress(tl::DayPeriod p, int& d, int& total) const {
  d = total = 0;
  for (int i = 0; i < KC_MAX_TASKS; ++i) {
    if (!used(i) || cfg_.tasks[i].period != p) continue;
    ++total;
    if (done(i)) ++d;
  }
}

void RoutineManager::progressAll(int& d, int& total) const {
  d = total = 0;
  for (int i = 0; i < KC_MAX_TASKS; ++i) {
    if (!used(i)) continue;
    ++total;
    if (done(i)) ++d;
  }
}

int RoutineManager::tasksIn(tl::DayPeriod p, int* out, int max) const {
  int n = 0;
  for (int i = 0; i < KC_MAX_TASKS && n < max; ++i)
    if (used(i) && cfg_.tasks[i].period == p) out[n++] = i;
  // insertion sort by time (untimed last)
  for (int a = 1; a < n; ++a) {
    const int v = out[a];
    const int key = cfg_.tasks[v].minute < 0 ? 9999 : cfg_.tasks[v].minute;
    int b = a - 1;
    while (b >= 0 && (cfg_.tasks[out[b]].minute < 0 ? 9999 : cfg_.tasks[out[b]].minute) > key) {
      out[b + 1] = out[b];
      --b;
    }
    out[b + 1] = v;
  }
  return n;
}

void RoutineManager::printStatus(Print& out) const {
  out.printf("  day %ld, stars today %u, total %lu\n", (long)st_.day, st_.starsToday, (unsigned long)st_.totalStars);
  for (int i = 0; i < KC_MAX_TASKS; ++i) {
    if (!used(i)) continue;
    const RoutineTask& t = cfg_.tasks[i];
    char hm[12] = "--:--";
    if (t.minute >= 0) tl::formatHM12AmPm(hm, sizeof hm, t.minute / 60, t.minute % 60);
    out.printf("  %2d [%c] %-9s %s %s%s\n", i + 1, done(i) ? 'x' : ' ', tl::periodName((tl::DayPeriod)t.period), hm, t.name,
               t.remind ? "" : " (no reminder)");
  }
}
