#include "TimeLogic.h"
#include <stdio.h>

namespace tl {

static const char* const kWeekdays[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char* const kWeekdaysShort[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char* const kMonths[12] = {"January", "February", "March", "April", "May", "June",
                                        "July", "August", "September", "October", "November", "December"};
static const char* const kMonthsShort[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

void to12h(int hour24, int& hour12, bool& pm) {
  hour24 = ((hour24 % 24) + 24) % 24;
  pm = hour24 >= 12;
  hour12 = hour24 % 12;
  if (hour12 == 0) hour12 = 12;
}

int to24h(int hour12, bool pm) {
  if (hour12 < 1) hour12 = 1;
  if (hour12 > 12) hour12 = 12;
  int h = hour12 % 12;  // 12 -> 0
  return pm ? h + 12 : h;
}

const char* ampm(int hour24) {
  hour24 = ((hour24 % 24) + 24) % 24;
  return hour24 >= 12 ? "PM" : "AM";
}

void formatHM12(char* out, size_t n, int hour24, int minute) {
  int h12; bool pm;
  to12h(hour24, h12, pm);
  snprintf(out, n, "%02d:%02d", h12, minute);
}

void formatHM12Short(char* out, size_t n, int hour24, int minute) {
  int h12; bool pm;
  to12h(hour24, h12, pm);
  snprintf(out, n, "%d:%02d", h12, minute);
}

void formatHM12AmPm(char* out, size_t n, int hour24, int minute) {
  int h12; bool pm;
  to12h(hour24, h12, pm);
  snprintf(out, n, "%02d:%02d %s", h12, minute, pm ? "PM" : "AM");
}

const char* weekdayName(int wday) { return kWeekdays[((wday % 7) + 7) % 7]; }
const char* weekdayShort(int wday) { return kWeekdaysShort[((wday % 7) + 7) % 7]; }
const char* monthName(int month) { return (month >= 1 && month <= 12) ? kMonths[month - 1] : "?"; }
const char* monthShort(int month) { return (month >= 1 && month <= 12) ? kMonthsShort[month - 1] : "?"; }

void formatDate(char* out, size_t n, int year, int month, int day) {
  snprintf(out, n, "%02d %s %04d", day, monthShort(month), year);
}

void formatDateLong(char* out, size_t n, int wday, int year, int month, int day) {
  snprintf(out, n, "%s, %02d %s %04d", weekdayName(wday), day, monthShort(month), year);
}

bool isLeap(int year) { return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0); }

int daysInMonth(int year, int month) {
  static const int d[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return 30;
  return (month == 2 && isLeap(year)) ? 29 : d[month - 1];
}

// Howard Hinnant's days_from_civil.
int32_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int32_t>(doe) - 719468;
}

static int wdayFromDays(int32_t days) {
  // 1970-01-01 was a Thursday (4).
  int w = static_cast<int>((days + 4) % 7);
  return w < 0 ? w + 7 : w;
}

int weekdayOf(int year, int month, int day) { return wdayFromDays(daysFromCivil(year, month, day)); }

bool inWindow(int nowMin, int startMin, int endMin) {
  if (startMin == endMin) return false;
  if (startMin < endMin) return nowMin >= startMin && nowMin < endMin;
  return nowMin >= startMin || nowMin < endMin;  // crosses midnight
}

static int64_t floorDiv(int64_t a, int64_t b) {
  int64_t q = a / b;
  if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
  return q;
}

LocalParts localFromEpochMinute(int64_t epochMinute, int offsetMin) {
  const int64_t local = epochMinute + offsetMin;
  LocalParts p;
  p.dayNumber = static_cast<int32_t>(floorDiv(local, 1440));
  const int mod = static_cast<int>(local - static_cast<int64_t>(p.dayNumber) * 1440);
  p.hour = mod / 60;
  p.minute = mod % 60;
  p.wday = wdayFromDays(p.dayNumber);
  return p;
}

int32_t routineDay(int64_t epochSeconds, int offsetMin, int resetHour) {
  const int64_t localMin = floorDiv(epochSeconds, 60) + offsetMin - static_cast<int64_t>(resetHour) * 60;
  return static_cast<int32_t>(floorDiv(localMin, 1440));
}

DayPeriod periodForHour(int h) {
  if (h >= 5 && h < 12) return PERIOD_MORNING;
  if (h >= 12 && h < 17) return PERIOD_AFTERNOON;
  if (h >= 17 && h < 21) return PERIOD_EVENING;
  return PERIOD_NIGHT;
}

const char* periodName(DayPeriod p) {
  switch (p) {
    case PERIOD_MORNING: return "Morning";
    case PERIOD_AFTERNOON: return "Afternoon";
    case PERIOD_EVENING: return "Evening";
    default: return "Night";
  }
}

}  // namespace tl
