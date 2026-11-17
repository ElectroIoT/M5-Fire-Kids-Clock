#include "BatteryManager.h"
#include <M5Unified.h>
#include "../config/Config.h"

BatteryManager Battery;

void BatteryManager::begin() {
  // Keep the IP5306 boost converter on at light load: without this the IP5306
  // can switch the whole clock off after ~30 s on battery when the screen is dim.
  if (M5.Power.getType() == m5::Power_Class::pmic_ip5306) {
    M5.Power.Ip5306.setPowerBoostKeepOn(true);
  }
  lastMs_ = millis() - 10000;
  loop();
  KC_LOG("Battery: %d%%, charging=%d, %d mV", level_, charging_, mv_);
}

void BatteryManager::loop() {
  const uint32_t ms = millis();
  if (ms - lastMs_ < 5000) return;
  lastMs_ = ms;
  const int lvl = M5.Power.getBatteryLevel();
  const bool chg = M5.Power.isCharging();
  mv_ = M5.Power.getBatteryVoltage();
  if (lvl != level_ || chg != charging_) {
    level_ = lvl;
    charging_ = chg;
    ++changes_;
  }
}
