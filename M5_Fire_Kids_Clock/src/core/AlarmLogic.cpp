#include "AlarmLogic.h"
#include <stdio.h>
#include <string.h>

namespace al {

const char* repeatName(uint8_t r) {
  switch (r) {
    case REPEAT_ONCE: return "Once";
    case REPEAT_DAILY: return "Every day";
    case REPEAT_WEEKDAYS: return "School days";
    case REPEAT_WEEKENDS: return "Weekends";
    case REPEAT_CUSTOM: return "Pick days";
    default: return "?";
  }
}

uint8_t effectiveDays(const AlarmConfig& a) {
  switch (a.repeat) {
    case REPEAT_ONCE:
    case REPEAT_DAILY: return DAYS_ALL;
    case REPEAT_WEEKDAYS: return DAYS_WEEKDAYS;
    case REPEAT_WEEKENDS: return DAYS_WEEKENDS;
    case REPEAT_CUSTOM: return a.days & DAYS_ALL;
    default: return 0;
  }
}

bool sanitize(AlarmConfig& a) {
  bool fixed = false;
  if (a.enabled > 1) { a.enabled = 1; fixed = true; }
  if (a.hour > 23) { a.hour = 7; fixed = true; }
  if (a.minute > 59) { a.minute = 0; fixed = true; }
  if (a.repeat >= REPEAT_COUNT) { a.repeat = REPEAT_ONCE; fixed = true; }
  if (a.days & 0x80) { a.days &= DAYS_ALL; fixed = true; }
  a.label[LABEL_LEN - 1] = '\0';
  return fixed;
}

bool matches(const AlarmConfig& a, int wday, int hour, int minute) {
  if (a.hour != hour || a.minute != minute) return false;
  return (effectiveDays(a) >> (wday % 7)) & 1;
}

int minutesUntilNext(const AlarmConfig& a, int wday, int hour, int minute) {
  if (!a.enabled) return -1;
  const uint8_t days = effectiveDays(a);
  if (!days) return -1;
  const int nowMin = hour * 60 + minute;
  const int alarmMin = a.hour * 60 + a.minute;
  for (int d = 0; d <= 7; ++d) {
    const int wd = (wday + d) % 7;
    if (!((days >> wd) & 1)) continue;
    const int delta = d * 1440 + alarmMin - nowMin;
    if (delta > 0) return delta;
  }
  return -1;
}

void describeDays(const AlarmConfig& a, char* out, size_t n) {
  if (!n) return;
  if (a.repeat != REPEAT_CUSTOM) {
    const char* s = a.repeat == REPEAT_WEEKDAYS ? "Mon-Fri" : a.repeat == REPEAT_WEEKENDS ? "Sat & Sun" : repeatName(a.repeat);
    snprintf(out, n, "%s", s);
    return;
  }
  const uint8_t d = a.days & DAYS_ALL;
  if (d == 0) { snprintf(out, n, "No days"); return; }
  if (d == DAYS_ALL) { snprintf(out, n, "Every day"); return; }
  static const char* const k2[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
  out[0] = '\0';
  size_t len = 0;
  // Monday-first order reads more naturally on a school schedule.
  static const int order[7] = {1, 2, 3, 4, 5, 6, 0};
  for (int i = 0; i < 7; ++i) {
    const int wd = order[i];
    if (!((d >> wd) & 1)) continue;
    int w = snprintf(out + len, n - len, "%s%s", len ? " " : "", k2[wd]);
    if (w < 0 || static_cast<size_t>(w) >= n - len) break;
    len += static_cast<size_t>(w);
  }
}

}  // namespace al
