// Battery / charging state from the Fire's I2C IP5306 (via M5Unified Power).
// The IP5306 only reports coarse steps (0/25/50/75/100 %). Polled every few
// seconds; the UI only redraws when a value changes.
#pragma once
#include <Arduino.h>

class BatteryManager {
 public:
  void begin();
  void loop();
  int level() const { return level_; }          // 0..100, -1 unknown
  bool charging() const { return charging_; }
  int millivolts() const { return mv_; }        // may be <= 0 if not reported
  bool low() const { return level_ >= 0 && level_ <= 25 && !charging_; }
  bool critical() const { return level_ >= 0 && level_ <= 5 && !charging_; }
  uint32_t changeCount() const { return changes_; }

 private:
  int level_ = -1;
  bool charging_ = false;
  int mv_ = 0;
  uint32_t lastMs_ = 0;
  uint32_t changes_ = 0;
};

extern BatteryManager Battery;
