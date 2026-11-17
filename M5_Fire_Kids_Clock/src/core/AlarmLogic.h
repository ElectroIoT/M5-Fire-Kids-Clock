// Pure alarm scheduling rules - no Arduino dependencies, unit-tested on the host.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace al {

constexpr int LABEL_LEN = 24;  // incl. terminator (matches KC_LABEL_LEN)

enum Repeat : uint8_t { REPEAT_ONCE = 0, REPEAT_DAILY, REPEAT_WEEKDAYS, REPEAT_WEEKENDS, REPEAT_CUSTOM, REPEAT_COUNT };

// Day bits use tm_wday numbering: bit0 = Sunday ... bit6 = Saturday.
constexpr uint8_t DAYS_ALL      = 0x7F;
constexpr uint8_t DAYS_WEEKDAYS = 0x3E;  // Mon..Fri
constexpr uint8_t DAYS_WEEKENDS = 0x41;  // Sun + Sat

// Stored as a raw blob in NVS - keep it POD and append-only between versions.
struct AlarmConfig {
  uint8_t enabled;
  uint8_t hour;     // 0..23 (internal 24-hour; shown as 12-hour AM/PM)
  uint8_t minute;   // 0..59
  uint8_t repeat;   // Repeat
  uint8_t days;     // custom day mask (used when repeat == REPEAT_CUSTOM)
  uint8_t sound;    // AudioManager sound index
  uint8_t reserved[2];
  char label[LABEL_LEN];
};

const char* repeatName(uint8_t repeat);
uint8_t effectiveDays(const AlarmConfig& a);
bool sanitize(AlarmConfig& a);  // clamps fields; returns true if something was fixed

// Does this alarm ring at (wday, hour, minute)? Ignores `enabled`.
bool matches(const AlarmConfig& a, int wday, int hour, int minute);

// Minutes from (wday, hour, minute) to the next ring strictly after now
// (1 .. 7*1440), or -1 if disabled / no days selected.
int minutesUntilNext(const AlarmConfig& a, int wday, int hour, int minute);

// Short description of the repeat rule: "Once", "Every day", "Mon-Fri",
// "Sat & Sun", "Mo We Fr".
void describeDays(const AlarmConfig& a, char* out, size_t n);

}  // namespace al
