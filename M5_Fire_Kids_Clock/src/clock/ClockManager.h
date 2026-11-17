// Timekeeping: ESP32 system time + SNTP (pool.ntp.org / time.google.com),
// Asia/Kolkata (IST, UTC+05:30, no DST). Works fully offline once the time is
// set; an optional external RTC can restore time after a complete power loss.
#pragma once
#include <Arduino.h>
#include <time.h>

struct LocalTime {
  int year, month, day;   // month 1..12
  int hour, minute, second;  // 24-hour internally (UI always shows 12-hour AM/PM)
  int wday;               // 0 = Sunday
  bool valid;             // false until time has been set (NTP, manual, RTC)
};

enum class TimeSource : uint8_t { None = 0, Ntp, Manual, Rtc, Retained };

class ClockManager {
 public:
  void begin();
  void loop();

  // Called by the Wi-Fi layer.
  void onNetworkUp();
  void onNetworkDown();
  void requestSync();          // force an SNTP round now (if online)

  bool isValid() const;        // system clock holds a plausible date
  bool everSynced() const;     // NTP succeeded since power-on (or soft reset)
  bool syncing() const { return syncing_; }
  TimeSource source() const { return source_; }
  const char* sourceName() const;
  time_t lastSyncEpoch() const { return lastSync_; }

  time_t nowEpoch() const { return time(nullptr); }
  LocalTime now() const;

  // Manual time set from the parent menu: 24-hour values, IST.
  bool setManual(int year, int month, int day, int hour24, int minute);

  // Monotonic counter that changes whenever the clock is stepped (sync/manual).
  uint32_t stepCount() const { return steps_; }

  void printStatus(Print& out) const;

  // Internal: SNTP callback entry.
  void handleSyncEvent(const struct timeval* tv);

 private:
  bool epochValid(time_t t) const;
  void startSntp();

  bool sntpStarted_ = false;
  bool online_ = false;
  volatile bool syncing_ = false;
  volatile bool syncFlag_ = false;
  TimeSource source_ = TimeSource::None;
  time_t lastSync_ = 0;
  uint32_t lastSyncMs_ = 0;
  uint32_t lastGoodMs_ = 0;
  time_t lastGoodEpoch_ = 0;
  uint32_t steps_ = 0;
  uint32_t lastRtcWriteMs_ = 0;
};

extern ClockManager Clock;
