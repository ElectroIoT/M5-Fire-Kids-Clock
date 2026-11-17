// Pure time helpers - no Arduino dependencies, unit-tested on the host (see /test).
//
// Rule: every time shown to the child is 12-hour with AM/PM. Internally the
// firmware keeps 24-hour values (0..23) for all calculations and storage.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace tl {

// India Standard Time is a fixed UTC+05:30 offset with no daylight saving.
constexpr int IST_OFFSET_MIN = 330;

// ---- 12 / 24 hour conversion ------------------------------------------------
// 0 -> 12 AM, 1..11 -> AM, 12 -> 12 PM, 13..23 -> 1..11 PM
void to12h(int hour24, int& hour12, bool& pm);
int  to24h(int hour12, bool pm);   // hour12 1..12
const char* ampm(int hour24);      // "AM" / "PM"

// "07:30"  (hour zero padded, 12-hour)
void formatHM12(char* out, size_t n, int hour24, int minute);
// "7:30"   (no leading zero, 12-hour)
void formatHM12Short(char* out, size_t n, int hour24, int minute);
// "07:30 AM"
void formatHM12AmPm(char* out, size_t n, int hour24, int minute);

// ---- Names -------------------------------------------------------------------
const char* weekdayName(int wday);   // 0 = Sunday
const char* weekdayShort(int wday);  // "Sun"
const char* monthName(int month);    // 1 = January
const char* monthShort(int month);   // "Jan"

// "17 Nov 2026"  (DD MMM YYYY)
void formatDate(char* out, size_t n, int year, int month, int day);
// "Tuesday, 17 Nov 2026"
void formatDateLong(char* out, size_t n, int wday, int year, int month, int day);

// ---- Calendar ----------------------------------------------------------------
int  daysInMonth(int year, int month);
bool isLeap(int year);
// Day of week for a civil date (0 = Sunday).
int  weekdayOf(int year, int month, int day);
// Days since 1970-01-01 for a civil date (proleptic Gregorian).
int32_t daysFromCivil(int year, int month, int day);

// ---- Minute-of-day windows -----------------------------------------------
inline int minuteOfDay(int hour24, int minute) { return hour24 * 60 + minute; }
// True when nowMin is inside [startMin, endMin). Handles windows that cross
// midnight (e.g. 21:00 -> 06:30). start == end means "never".
bool inWindow(int nowMin, int startMin, int endMin);

// ---- Fixed-offset local time from epoch -----------------------------------
struct LocalParts {
  int32_t dayNumber;  // local days since 1970-01-01
  int wday;           // 0 = Sunday
  int hour;           // 0..23
  int minute;         // 0..59
};
// epochMinute = UNIX seconds / 60 (floor). offsetMin = +330 for IST.
LocalParts localFromEpochMinute(int64_t epochMinute, int offsetMin);

// Routine "day" that starts at resetHour (e.g. 3 -> the day changes at 3:00 AM).
int32_t routineDay(int64_t epochSeconds, int offsetMin, int resetHour);

// ---- Day period for characters / routine --------------------------------
enum DayPeriod : uint8_t { PERIOD_MORNING = 0, PERIOD_AFTERNOON, PERIOD_EVENING, PERIOD_NIGHT, PERIOD_COUNT };
DayPeriod periodForHour(int hour24);   // 5-11 morning, 12-16 afternoon, 17-20 evening, else night
const char* periodName(DayPeriod p);

}  // namespace tl
