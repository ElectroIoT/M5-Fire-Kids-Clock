#include "EventLog.h"
#include <esp_system.h>
#include <time.h>

RTC_NOINIT_ATTR static uint32_t g_boots;
RTC_NOINIT_ATTR static uint32_t g_bootsMagic;

const char* resetReasonText() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_EXT: return "reset pin";
    case ESP_RST_SW: return "software restart";
    case ESP_RST_PANIC: return "crash (panic)";
    case ESP_RST_INT_WDT: return "watchdog (interrupt)";
    case ESP_RST_TASK_WDT: return "watchdog (task)";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    case ESP_RST_BROWNOUT: return "brownout (power dip)";
    case ESP_RST_SDIO: return "sdio";
    default: return "unknown";
  }
}

uint32_t bootCount() {
  static bool counted = false;
  if (!counted) {
    counted = true;
    const esp_reset_reason_t r = esp_reset_reason();
    if (g_bootsMagic != 0xB0075EEDu || r == ESP_RST_POWERON) {
      g_bootsMagic = 0xB0075EEDu;
      g_boots = 0;
    }
    ++g_boots;
  }
  return g_boots;
}

EventLog Events;

void EventLog::add(EventType t, int index, const char* text) {
  Event& e = ring_[seq_ % kSize];
  e.seq = ++seq_;
  const time_t now = time(nullptr);
  e.epoch = now > 1700000000 ? (uint32_t)now : 0;
  e.type = t;
  e.index = (int8_t)index;
  strlcpy(e.text, text ? text : "", sizeof e.text);
}

int EventLog::since(uint32_t after, Event* out, int max) const {
  int n = 0;
  const uint32_t first = seq_ > (uint32_t)kSize ? seq_ - kSize + 1 : 1;
  for (uint32_t s = (after + 1 > first ? after + 1 : first); s <= seq_ && n < max; ++s) {
    out[n++] = ring_[(s - 1) % kSize];
  }
  return n;
}

const char* EventLog::typeName(EventType t) {
  switch (t) {
    case EventType::Boot: return "boot";
    case EventType::AlarmRing: return "alarm_ring";
    case EventType::AlarmSnooze: return "alarm_snooze";
    case EventType::AlarmStop: return "alarm_stop";
    case EventType::AlarmIgnored: return "alarm_ignored";
    case EventType::RoutineDone: return "routine_done";
    case EventType::RoutinePeriodDone: return "routine_period_done";
    case EventType::RoutineAllDone: return "routine_all_done";
    case EventType::BatteryLow: return "battery_low";
    case EventType::PowerWeak: return "power_weak";
    default: return "time_lost";
  }
}
