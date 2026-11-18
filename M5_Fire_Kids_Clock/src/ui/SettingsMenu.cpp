#include "SettingsMenu.h"
#include "../config/Config.h"
#include "../core/TimeLogic.h"
#include "../settings/SettingsManager.h"
#include "../clock/ClockManager.h"
#include "../wifi/WiFiLink.h"
#include "../display/DisplayManager.h"
#include "../storage/StorageManager.h"
#include "../app/NightMode.h"
#include "../weather/WeatherManager.h"
#include <math.h>
#include "../routine/RoutineManager.h"
#include "../audio/AudioManager.h"
#include "../led/LEDManager.h"

namespace settingsmenu {

static void onOff(char* b, size_t n, bool v) { snprintf(b, n, "%s", v ? "On" : "Off"); }

static void time12(char* b, size_t n, int minuteOfDay) {
  tl::formatHM12AmPm(b, n, minuteOfDay / 60, minuteOfDay % 60);
}

static uint8_t step(uint8_t v, int dir, uint8_t lo, uint8_t hi) {
  int x = (int)v + dir;
  return (uint8_t)(x < lo ? lo : (x > hi ? hi : x));
}

static uint16_t stepMinutes(uint16_t m, int dir, int stepMin) {
  int x = ((int)m + dir * stepMin) % 1440;
  if (x < 0) x += 1440;
  return (uint16_t)x;
}

static Settings& S() { return AppCfg.get(); }

void build(std::vector<MenuItem>& items) {
  items.push_back({"Back", MenuItem::Action, nullptr, nullptr, [] { UI.back(); }});

  // ---- Display ------------------------------------------------------------
  items.push_back({"Display", MenuItem::Header, nullptr, nullptr, nullptr});
  items.push_back({"Brightness", MenuItem::Value, [](char* b, size_t n) { snprintf(b, n, "%d / 10", S().brightness); },
                   [](int d) { S().brightness = step(S().brightness, d, 1, 10); AppCfg.markDirty(); }, nullptr});
  items.push_back({"Dim when idle", MenuItem::Value,
                   [](char* b, size_t n) {
                     if (S().idleDimMin) snprintf(b, n, "after %d min", S().idleDimMin);
                     else snprintf(b, n, "Never");
                   },
                   [](int d) {
                     static const uint8_t opts[] = {0, 1, 5, 15, 30};
                     int i = 0;
                     while (i < 4 && opts[i] != S().idleDimMin) ++i;
                     i = i + d < 0 ? 0 : (i + d > 4 ? 4 : i + d);
                     S().idleDimMin = opts[i];
                     AppCfg.markDirty();
                   },
                   nullptr});

  // ---- Sound -----------------------------------------------------------------
  items.push_back({"Sound", MenuItem::Header, nullptr, nullptr, nullptr});
  items.push_back({"Volume", MenuItem::Value, [](char* b, size_t n) { snprintf(b, n, "%d / 10", S().volume); },
                   [](int d) {
                     S().volume = step(S().volume, d, 0, 10);
                     AppCfg.markDirty();
                     Audio.applyVolume();
                     Audio.effect(sounds::FX_NOTIFY);
                   },
                   nullptr});
  items.push_back({"Quiet mode", MenuItem::Toggle, [](char* b, size_t n) { onOff(b, n, S().muted); },
                   [](int) {
                     S().muted ^= 1;
                     AppCfg.markDirty();
                     UI.toast(S().muted ? "Beeps off (alarms still ring)" : "Beeps on");
                   },
                   nullptr});
  items.push_back({"Button beeps", MenuItem::Toggle, [](char* b, size_t n) { onOff(b, n, S().clickSounds); },
                   [](int) { S().clickSounds ^= 1; AppCfg.markDirty(); }, nullptr});
  items.push_back({"Alarm fade-in", MenuItem::Toggle, [](char* b, size_t n) { onOff(b, n, S().gradualVolume); },
                   [](int) { S().gradualVolume ^= 1; AppCfg.markDirty(); }, nullptr});
  items.push_back({"Bedtime lullaby", MenuItem::Toggle, [](char* b, size_t n) { onOff(b, n, S().bedtimeMusic); },
                   [](int) { S().bedtimeMusic ^= 1; AppCfg.markDirty(); }, nullptr});
  items.push_back({"microSD sounds", MenuItem::Action,
                   [](char* b, size_t n) { snprintf(b, n, "%s", Audio.sdReady() ? "card found" : "no card"); }, nullptr,
                   [] { UI.toast(Audio.sdReady() ? "Put WAVs in /sounds" : "Insert a FAT32 card + restart"); }});

  // ---- Lights ----------------------------------------------------------------
  items.push_back({"Lights", MenuItem::Header, nullptr, nullptr, nullptr});
  items.push_back({"LED lights", MenuItem::Toggle, [](char* b, size_t n) { onOff(b, n, S().ledEnabled); },
                   [](int) { S().ledEnabled ^= 1; AppCfg.markDirty(); }, nullptr});
  items.push_back({"LED brightness", MenuItem::Value, [](char* b, size_t n) { snprintf(b, n, "%d / 10", S().ledBrightness); },
                   [](int d) {
                     S().ledBrightness = step(S().ledBrightness, d, 1, 10);
                     AppCfg.markDirty();
                     Leds.flash(LEDManager::Effect::Morning, 1500);
                   },
                   nullptr});
  items.push_back({"Night light glow", MenuItem::Toggle, [](char* b, size_t n) { onOff(b, n, S().ledNightGlow); },
                   [](int) { S().ledNightGlow ^= 1; AppCfg.markDirty(); }, nullptr});
  items.push_back({"Rainbow test", MenuItem::Action, nullptr, nullptr, [] {
                     Leds.flash(LEDManager::Effect::Reward, 3000);
                     Audio.effect(sounds::FX_REWARD);
                   }});

  // ---- Alarms ----------------------------------------------------------------
  items.push_back({"Alarms", MenuItem::Header, nullptr, nullptr, nullptr});
  items.push_back({"Edit alarms", MenuItem::Action, nullptr, nullptr, [] { UI.go(ScreenId::AlarmList); }});
  items.push_back({"Snooze time", MenuItem::Value, [](char* b, size_t n) { snprintf(b, n, "%d min", S().snoozeMin); },
                   [](int d) {
                     static const uint8_t opts[] = {5, 10, 15};
                     int i = S().snoozeMin == 15 ? 2 : (S().snoozeMin == 10 ? 1 : 0);
                     i = i + d < 0 ? 0 : (i + d > 2 ? 2 : i + d);
                     S().snoozeMin = opts[i];
                     AppCfg.markDirty();
                   },
                   nullptr});

  // ---- Night mode ----------------------------------------------------------
  items.push_back({"Night mode", MenuItem::Header, nullptr, nullptr, nullptr});
  items.push_back({"Auto night mode", MenuItem::Toggle, [](char* b, size_t n) { onOff(b, n, S().nightEnabled); },
                   [](int) { S().nightEnabled ^= 1; AppCfg.markDirty(); }, nullptr});
  items.push_back({"Night starts", MenuItem::Value, [](char* b, size_t n) { time12(b, n, S().nightStartMin); },
                   [](int d) { S().nightStartMin = stepMinutes(S().nightStartMin, d, 15); AppCfg.markDirty(); }, nullptr});
  items.push_back({"Night ends", MenuItem::Value, [](char* b, size_t n) { time12(b, n, S().nightEndMin); },
                   [](int d) { S().nightEndMin = stepMinutes(S().nightEndMin, d, 15); AppCfg.markDirty(); }, nullptr});
  items.push_back({"Night brightness", MenuItem::Value, [](char* b, size_t n) { snprintf(b, n, "%d / 5", S().nightBrightness); },
                   [](int d) { S().nightBrightness = step(S().nightBrightness, d, 1, 5); AppCfg.markDirty(); }, nullptr});
  items.push_back({"Wi-Fi off at night", MenuItem::Toggle, [](char* b, size_t n) { onOff(b, n, S().nightWifiOff); },
                   [](int) { S().nightWifiOff ^= 1; AppCfg.markDirty(); }, nullptr});
  items.push_back({"Sleep now", MenuItem::Action, nullptr, nullptr, [] {
                     NightMode.forceNow(true);
                     UI.home();
                     UI.toast("Good night!");
                   }});

  // ---- Time & Wi-Fi --------------------------------------------------------
  items.push_back({"Time & Wi-Fi", MenuItem::Header, nullptr, nullptr, nullptr});
  items.push_back({"Time format", MenuItem::Action, [](char* b, size_t n) { snprintf(b, n, "12-hour AM/PM"); }, nullptr,
                   [] { UI.toast("Kids Clock always uses AM/PM"); }});
  items.push_back({"Set time & date", MenuItem::Action, nullptr, nullptr, [] { UI.go(ScreenId::SetTime); }});
  items.push_back({"Wi-Fi", MenuItem::Toggle, [](char* b, size_t n) { onOff(b, n, S().wifiEnabled); },
                   [](int) {
                     S().wifiEnabled ^= 1;
                     AppCfg.save();
                     Net.applyEnabled();
                   },
                   nullptr});
  items.push_back({"Parent setup (phone)", MenuItem::Action, nullptr, nullptr, [] { UI.go(ScreenId::WifiSetup); }});
  items.push_back({"Pair phone app", MenuItem::Action, nullptr, nullptr, [] { UI.go(ScreenId::Pair); }});
  items.push_back({"Weather", MenuItem::Toggle, [](char* b, size_t n) { onOff(b, n, S().weatherEnabled); },
                   [](int) {
                     S().weatherEnabled ^= 1;
                     AppCfg.markDirty();
                     Weather.refreshSoon();
                   },
                   nullptr});
  items.push_back({"Weather place", MenuItem::Action,
                   [](char* b, size_t n) {
                     if (isnan(S().latitude)) snprintf(b, n, "set on phone");
                     else if (S().city[0]) snprintf(b, n, "%s", S().city);
                     else snprintf(b, n, "%.2f, %.2f", S().latitude, S().longitude);
                   },
                   nullptr, [] { UI.go(ScreenId::WifiSetup); }});
  items.push_back({"Sync time now", MenuItem::Action,
                   [](char* b, size_t n) { snprintf(b, n, "%s", Clock.syncing() ? "syncing..." : Net.connected() ? "" : "offline"); },
                   nullptr, [] {
                     if (Net.connected()) {
                       Clock.requestSync();
                       UI.toast("Checking the time...");
                     } else {
                       UI.toast("No Wi-Fi connection");
                     }
                   }});
  items.push_back({"Warn if not synced", MenuItem::Toggle, [](char* b, size_t n) { onOff(b, n, S().showSyncWarning); },
                   [](int) { S().showSyncWarning ^= 1; AppCfg.markDirty(); }, nullptr});

  // ---- Routine -----------------------------------------------------------------
  items.push_back({"Daily routine", MenuItem::Header, nullptr, nullptr, nullptr});
  items.push_back({"Job reminders", MenuItem::Toggle, [](char* b, size_t n) { onOff(b, n, S().routineReminders); },
                   [](int) { S().routineReminders ^= 1; AppCfg.markDirty(); }, nullptr});
  items.push_back({"New day starts", MenuItem::Value,
                   [](char* b, size_t n) { tl::formatHM12AmPm(b, n, S().routineResetHour, 0); },
                   [](int d) {
                     S().routineResetHour = (uint8_t)((S().routineResetHour + d + 24) % 24);
                     AppCfg.markDirty();
                   },
                   nullptr});
  items.push_back({"Edit jobs", MenuItem::Action, [](char* b, size_t n) { snprintf(b, n, "on phone"); }, nullptr,
                   [] { UI.go(ScreenId::WifiSetup); }});
  items.push_back({"Reset stars", MenuItem::Action, nullptr, nullptr, [] {
                     ConfirmDlg.ask("Reset stars?", "All stars and badges\ngo back to zero.", [] {
                       Routine.resetStars();
                       UI.toast("Stars reset");
                     });
                   }});

  // ---- System ----------------------------------------------------------------
  items.push_back({"System", MenuItem::Header, nullptr, nullptr, nullptr});
  items.push_back({"Device info", MenuItem::Action, nullptr, nullptr, [] { UI.go(ScreenId::Info); }});
  items.push_back({"Reset all settings", MenuItem::Action, nullptr, nullptr, [] {
                     ConfirmDlg.ask("Reset everything?", "Alarms, routine, stars and\nWi-Fi will be erased.\nThe clock restarts.", [] {
                       Storage.eraseAll();
                       KC_LOG("Factory reset - restarting");
                       delay(300);
                       ESP.restart();
                     });
                   }});
}

}  // namespace settingsmenu
