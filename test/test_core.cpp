// Host unit tests for the pure time / alarm / routine logic.
// Build & run (Linux, WSL or macOS):
//   g++ -std=c++17 -Wall -Wextra -I../M5_Fire_Kids_Clock/src test_core.cpp
//       ../M5_Fire_Kids_Clock/src/core/TimeLogic.cpp ../M5_Fire_Kids_Clock/src/core/AlarmLogic.cpp -o test_core && ./test_core
#include <cstdio>
#include <cstring>
#include "core/TimeLogic.h"
#include "core/AlarmLogic.h"

static int g_fail = 0, g_pass = 0;
#define CHECK(c) do { if (c) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_STR(a, b) do { if (std::strcmp((a), (b)) == 0) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d  \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); } } while (0)

// UNIX seconds for an IST wall-clock time.
static long long ist(int y, int mo, int d, int h, int mi) {
  return (long long)tl::daysFromCivil(y, mo, d) * 86400LL + h * 3600LL + mi * 60LL - tl::IST_OFFSET_MIN * 60LL;
}

static void test12h() {
  char b[16];
  tl::formatHM12AmPm(b, sizeof b, 0, 0);   CHECK_STR(b, "12:00 AM");   // midnight
  tl::formatHM12AmPm(b, sizeof b, 0, 1);   CHECK_STR(b, "12:01 AM");
  tl::formatHM12AmPm(b, sizeof b, 11, 59); CHECK_STR(b, "11:59 AM");
  tl::formatHM12AmPm(b, sizeof b, 12, 0);  CHECK_STR(b, "12:00 PM");   // noon
  tl::formatHM12AmPm(b, sizeof b, 12, 30); CHECK_STR(b, "12:30 PM");
  tl::formatHM12AmPm(b, sizeof b, 13, 5);  CHECK_STR(b, "01:05 PM");
  tl::formatHM12AmPm(b, sizeof b, 7, 30);  CHECK_STR(b, "07:30 AM");
  tl::formatHM12AmPm(b, sizeof b, 18, 45); CHECK_STR(b, "06:45 PM");
  tl::formatHM12AmPm(b, sizeof b, 21, 15); CHECK_STR(b, "09:15 PM");
  tl::formatHM12AmPm(b, sizeof b, 23, 59); CHECK_STR(b, "11:59 PM");
  tl::formatHM12Short(b, sizeof b, 0, 5);  CHECK_STR(b, "12:05");
  for (int h = 0; h < 24; ++h) {
    int h12; bool pm; tl::to12h(h, h12, pm);
    CHECK(h12 >= 1 && h12 <= 12);
    CHECK(tl::to24h(h12, pm) == h);  // round trip for every hour
  }
  CHECK(tl::to24h(12, false) == 0);
  CHECK(tl::to24h(12, true) == 12);
  CHECK_STR(tl::ampm(0), "AM");
  CHECK_STR(tl::ampm(12), "PM");
}

static void testDates() {
  char b[40];
  tl::formatDateLong(b, sizeof b, tl::weekdayOf(2026, 11, 17), 2026, 11, 17);
  CHECK_STR(b, "Tuesday, 17 Nov 2026");
  tl::formatDate(b, sizeof b, 2027, 1, 3);
  CHECK_STR(b, "03 Jan 2027");
  CHECK(tl::weekdayOf(1970, 1, 1) == 4);
  CHECK(tl::weekdayOf(2000, 2, 29) == 2);
  CHECK(tl::daysInMonth(2028, 2) == 29);
  CHECK(tl::daysInMonth(2100, 2) == 28);
  CHECK(tl::daysInMonth(2026, 4) == 30);
}

static void testLocalParts() {
  // 17 Nov 2026 23:59 IST -> Tuesday 23:59
  auto p = tl::localFromEpochMinute(ist(2026, 11, 17, 23, 59) / 60, tl::IST_OFFSET_MIN);
  CHECK(p.wday == 2 && p.hour == 23 && p.minute == 59);
  // one minute later: Wednesday 00:00 (weekday transition at midnight)
  auto q = tl::localFromEpochMinute(ist(2026, 11, 17, 23, 59) / 60 + 1, tl::IST_OFFSET_MIN);
  CHECK(q.wday == 3 && q.hour == 0 && q.minute == 0);
  CHECK(q.dayNumber == p.dayNumber + 1);
  // UTC midnight is 05:30 IST
  auto r = tl::localFromEpochMinute(tl::daysFromCivil(2026, 11, 17) * 1440LL, tl::IST_OFFSET_MIN);
  CHECK(r.hour == 5 && r.minute == 30);
  // 18:30 UTC on the 16th is 00:00 IST on the 17th
  auto s = tl::localFromEpochMinute(tl::daysFromCivil(2026, 11, 16) * 1440LL + 18 * 60 + 30, tl::IST_OFFSET_MIN);
  CHECK(s.hour == 0 && s.minute == 0 && s.wday == 2);
}

static void testWindows() {
  const int s = 21 * 60, e = 6 * 60 + 30;  // 9:00 PM -> 6:30 AM
  CHECK(tl::inWindow(21 * 60, s, e));
  CHECK(tl::inWindow(23 * 60 + 59, s, e));
  CHECK(tl::inWindow(0, s, e));
  CHECK(tl::inWindow(6 * 60 + 29, s, e));
  CHECK(!tl::inWindow(6 * 60 + 30, s, e));
  CHECK(!tl::inWindow(12 * 60, s, e));
  CHECK(!tl::inWindow(20 * 60 + 59, s, e));
  CHECK(tl::inWindow(13 * 60, 13 * 60, 15 * 60));  // same-day window
  CHECK(!tl::inWindow(15 * 60, 13 * 60, 15 * 60));
  CHECK(!tl::inWindow(100, 300, 300));             // empty window
}

static void testRoutineDay() {
  // reset hour 3 AM: 2:59 AM still belongs to the previous routine day
  int32_t d1 = tl::routineDay(ist(2026, 11, 17, 22, 0), tl::IST_OFFSET_MIN, 3);
  int32_t d2 = tl::routineDay(ist(2026, 11, 18, 2, 59), tl::IST_OFFSET_MIN, 3);
  int32_t d3 = tl::routineDay(ist(2026, 11, 18, 3, 0), tl::IST_OFFSET_MIN, 3);
  CHECK(d1 == d2);
  CHECK(d3 == d1 + 1);
  // reset hour 0: midnight boundary
  CHECK(tl::routineDay(ist(2026, 11, 17, 23, 59), tl::IST_OFFSET_MIN, 0) + 1 ==
        tl::routineDay(ist(2026, 11, 18, 0, 0), tl::IST_OFFSET_MIN, 0));
}

static al::AlarmConfig mk(int h, int m, uint8_t rep, uint8_t days = 0) {
  al::AlarmConfig a{};
  a.enabled = 1; a.hour = h; a.minute = m; a.repeat = rep; a.days = days;
  std::strcpy(a.label, "Test");
  return a;
}

static void testAlarms() {
  // School wake-up 6:30 AM on weekdays
  auto school = mk(6, 30, al::REPEAT_WEEKDAYS);
  CHECK(al::matches(school, 1, 6, 30));    // Monday
  CHECK(al::matches(school, 5, 6, 30));    // Friday
  CHECK(!al::matches(school, 6, 6, 30));   // Saturday
  CHECK(!al::matches(school, 0, 6, 30));   // Sunday
  CHECK(!al::matches(school, 1, 18, 30));  // 6:30 PM is not 6:30 AM
  // Friday 7:00 AM -> next is Monday 6:30 AM
  CHECK(al::minutesUntilNext(school, 5, 7, 0) == 3 * 1440 - 30);
  // Monday 6:30 exactly -> strictly after now -> Tuesday 6:30
  CHECK(al::minutesUntilNext(school, 1, 6, 30) == 1440);
  CHECK(al::minutesUntilNext(school, 1, 6, 29) == 1);

  // Midnight alarm (12:00 AM)
  auto mid = mk(0, 0, al::REPEAT_DAILY);
  CHECK(al::matches(mid, 3, 0, 0));
  CHECK(al::minutesUntilNext(mid, 2, 23, 59) == 1);
  // Noon alarm (12:00 PM)
  auto noon = mk(12, 0, al::REPEAT_DAILY);
  CHECK(al::matches(noon, 0, 12, 0));
  CHECK(!al::matches(noon, 0, 0, 0));
  CHECK(al::minutesUntilNext(noon, 0, 11, 59) == 1);
  // 11:59 PM alarm
  auto late = mk(23, 59, al::REPEAT_ONCE);
  CHECK(al::matches(late, 4, 23, 59));
  CHECK(al::minutesUntilNext(late, 4, 0, 1) == 23 * 60 + 58);

  // Weekends
  auto wk = mk(9, 0, al::REPEAT_WEEKENDS);
  CHECK(al::matches(wk, 6, 9, 0) && al::matches(wk, 0, 9, 0) && !al::matches(wk, 3, 9, 0));
  // Custom: Mon + Wed
  auto cu = mk(17, 0, al::REPEAT_CUSTOM, (1 << 1) | (1 << 3));
  CHECK(al::matches(cu, 1, 17, 0) && al::matches(cu, 3, 17, 0) && !al::matches(cu, 2, 17, 0));
  CHECK(al::minutesUntilNext(cu, 3, 17, 1) == 5 * 1440 - 1);  // Wed 5:01 PM -> Mon 5:00 PM
  // Custom with no days never rings
  auto none = mk(8, 0, al::REPEAT_CUSTOM, 0);
  CHECK(al::minutesUntilNext(none, 1, 7, 0) == -1);
  // Disabled
  auto off = mk(8, 0, al::REPEAT_DAILY); off.enabled = 0;
  CHECK(al::minutesUntilNext(off, 1, 7, 0) == -1);

  char b[32];
  al::describeDays(cu, b, sizeof b);     CHECK_STR(b, "Mo We");
  al::describeDays(school, b, sizeof b); CHECK_STR(b, "Mon-Fri");

  // sanitize repairs corrupted data
  al::AlarmConfig bad; std::memset(&bad, 0xFF, sizeof bad);
  CHECK(al::sanitize(bad));
  CHECK(bad.hour <= 23 && bad.minute <= 59 && bad.repeat < al::REPEAT_COUNT && bad.enabled <= 1);
  CHECK(bad.label[al::LABEL_LEN - 1] == '\0');
}

int main() {
  test12h();
  testDates();
  testLocalParts();
  testWindows();
  testRoutineDay();
  testAlarms();
  std::printf("%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
