#include "SettingsManager.h"
#include <math.h>
#include "../config/Config.h"
#include "../storage/StorageManager.h"

SettingsManager AppCfg;

static constexpr uint16_t kSettingsVersion = 1;
static constexpr uint16_t kWifiVersion = 1;

void SettingsManager::defaults(Settings& s) {
  memset(&s, 0, sizeof s);
  s.volume = 6;
  s.muted = 0;
  s.defaultSound = 0;
  s.gradualVolume = 1;
  s.clickSounds = 1;
  s.snoozeMin = 5;
  s.brightness = 7;
  s.idleDimMin = 0;
  s.showSyncWarning = 1;
  s.nightEnabled = 1;
  s.nightStartMin = 21 * 60;      // 9:00 PM
  s.nightEndMin = 6 * 60 + 30;    // 6:30 AM
  s.nightBrightness = 1;
  s.bedtimeMusic = 1;
  s.nightWifiOff = 0;
  s.ledEnabled = 1;
  s.ledBrightness = 3;
  s.ledNightGlow = 0;
  s.wifiEnabled = 1;
  s.weatherEnabled = 1;
  s.latitude = NAN;
  s.longitude = NAN;
  s.city[0] = '\0';
  s.routineResetHour = 3;         // new routine day at 3:00 AM
  s.routineReminders = 1;
  s.childName[0] = '\0';
}

void SettingsManager::begin() {
  defaults(s_);
  auto r = Storage.loadBlob("settings", kSettingsVersion, &s_, sizeof s_);
  if (r == StorageManager::LoadResult::Corrupt) defaults(s_);
  sanitize();
  KC_LOG("Settings: %s", StorageManager::resultName(r));
  if (r == StorageManager::LoadResult::Migrated) save();

  memset(&wifi_, 0, sizeof wifi_);
  auto w = Storage.loadBlob("wifi", kWifiVersion, &wifi_, sizeof wifi_);
  if (w != StorageManager::LoadResult::Ok && w != StorageManager::LoadResult::Migrated) {
    memset(&wifi_, 0, sizeof wifi_);
    strlcpy(wifi_.ssid, KC_DEFAULT_SSID, sizeof wifi_.ssid);
    strlcpy(wifi_.pass, KC_DEFAULT_PASS, sizeof wifi_.pass);
  }
  wifi_.ssid[sizeof wifi_.ssid - 1] = '\0';
  wifi_.pass[sizeof wifi_.pass - 1] = '\0';
  wifi_.ssid2[sizeof wifi_.ssid2 - 1] = '\0';
  wifi_.pass2[sizeof wifi_.pass2 - 1] = '\0';
  KC_LOG("Wi-Fi creds: %s (%s%s)", StorageManager::resultName(w), wifi_.ssid[0] ? "network set" : "no network",
         wifi_.ssid2[0] ? " + backup" : "");
}

static uint8_t clampU8(uint8_t v, uint8_t lo, uint8_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

void SettingsManager::sanitize() {
  Settings d;
  defaults(d);
  s_.volume = clampU8(s_.volume, 0, 10);
  s_.muted = s_.muted ? 1 : 0;
  s_.gradualVolume = s_.gradualVolume ? 1 : 0;
  s_.clickSounds = s_.clickSounds ? 1 : 0;
  if (s_.snoozeMin != 5 && s_.snoozeMin != 10 && s_.snoozeMin != 15) s_.snoozeMin = 5;
  s_.brightness = clampU8(s_.brightness, 1, 10);
  if (s_.idleDimMin > 30) s_.idleDimMin = 0;
  if (s_.nightStartMin >= 1440) s_.nightStartMin = d.nightStartMin;
  if (s_.nightEndMin >= 1440) s_.nightEndMin = d.nightEndMin;
  s_.nightBrightness = clampU8(s_.nightBrightness, 1, 10);
  s_.ledBrightness = clampU8(s_.ledBrightness, 1, 10);
  if (!(s_.latitude >= -90.f && s_.latitude <= 90.f)) s_.latitude = NAN;
  if (!(s_.longitude >= -180.f && s_.longitude <= 180.f)) s_.longitude = NAN;
  s_.city[sizeof s_.city - 1] = '\0';
  if (s_.routineResetHour > 23) s_.routineResetHour = d.routineResetHour;
  s_.childName[sizeof s_.childName - 1] = '\0';
}

void SettingsManager::save() {
  sanitize();
  Storage.saveBlob("settings", kSettingsVersion, &s_, sizeof s_);
  dirty_ = false;
}

void SettingsManager::markDirty() {
  dirty_ = true;
  dirtySince_ = millis();
}

void SettingsManager::loop() {
  // Rapid A/C presses only cost one flash write, 2 s after the last change.
  if (dirty_ && millis() - dirtySince_ > 2000) save();
}

void SettingsManager::resetToDefaults() {
  defaults(s_);
  save();
}

void SettingsManager::saveWifi() {
  Storage.saveBlob("wifi", kWifiVersion, &wifi_, sizeof wifi_);
}
