// Small in-RAM event log for the companion app (notifications).
// Each event gets an increasing sequence number; the app asks for
// "everything after N" (GET /api/v1/events?after=N).
#pragma once
#include <Arduino.h>

enum class EventType : uint8_t {
  Boot,
  AlarmRing,
  AlarmSnooze,
  AlarmStop,
  AlarmIgnored,     // stopped by itself after auto-snoozes - nobody pressed a button
  RoutineDone,      // one job ticked
  RoutinePeriodDone,// all jobs of a part of the day
  RoutineAllDone,   // the whole day
  BatteryLow,
  TimeLost,
  PowerWeak,        // brownout restart: supply too weak (base / battery missing?)
};

struct Event {
  uint32_t seq;
  uint32_t epoch;   // UNIX time (0 if the clock had no valid time)
  EventType type;
  int8_t index;     // alarm / job number (0-based), -1 if not relevant
  char text[30];
};

class EventLog {
 public:
  static constexpr int kSize = 32;
  void add(EventType t, int index, const char* text);
  // Copies events with seq > after (oldest first). Returns how many.
  int since(uint32_t after, Event* out, int max) const;
  uint32_t lastSeq() const { return seq_; }
  static const char* typeName(EventType t);

 private:
  Event ring_[kSize] = {};
  uint32_t seq_ = 0;
};

extern EventLog Events;

// Why the clock last started ("power-on", "brownout", "panic", "watchdog", ...)
const char* resetReasonText();
// Restarts since the last power-on (kept in RTC memory across resets).
uint32_t bootCount();
