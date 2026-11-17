// Parent-adjustable settings, persisted in NVS (versioned blob, see StorageManager).
#pragma once
#include <Arduino.h>

struct Settings {
  // Sound
  uint8_t volume;           // 0..10
  uint8_t muted;            // 0/1
  uint8_t defaultSound;     // sound index for new alarms
  uint8_t gradualVolume;    // alarm fades in
  uint8_t clickSounds;      // button beeps
  uint8_t snoozeMin;        // 5 / 10 / 15
  // Display
  uint8_t brightness;       // 1..10
  uint8_t idleDimMin;       // 0 = never, else minutes before the screen dims (power saving)
  uint8_t showSyncWarning;  // show "time not set" banner
  // Night mode
  uint8_t nightEnabled;
  uint16_t nightStartMin;   // minute of day, e.g. 21*60
  uint16_t nightEndMin;     // minute of day, e.g. 6*60+30
  uint8_t nightBrightness;  // 1..10 (very low)
  uint8_t bedtimeMusic;     // lullaby when night mode starts
  uint8_t nightWifiOff;     // drop Wi-Fi during the night window
  // LEDs
  uint8_t ledEnabled;
  uint8_t ledBrightness;    // 1..10
  uint8_t ledNightGlow;     // dim purple night light while in night mode
  // Network
  uint8_t wifiEnabled;
  uint8_t weatherEnabled;
  float latitude;           // NAN = not configured
  float longitude;
  char city[24];
  // Routine
  uint8_t routineResetHour; // 0..23, new routine day starts here
  uint8_t routineReminders; // pop up reminders at task times
  // Personalisation (added in 1.0: appended so older saved settings migrate)
  char childName[16];       // shown in greetings; empty = generic messages
};

struct WifiCreds {
  char ssid[33];
  char pass[65];
  char ssid2[33];   // backup network, tried when the main one fails
  char pass2[65];
};

class SettingsManager {
 public:
  void begin();
  Settings& get() { return s_; }
  const Settings& get() const { return s_; }
  void save();                 // persist immediately
  void markDirty();            // persist after a short delay (coalesces rapid edits)
  void loop();
  void resetToDefaults();      // settings only (alarms/routine handled by their managers)

  WifiCreds& wifi() { return wifi_; }
  void saveWifi();

  static void defaults(Settings& s);

 private:
  void sanitize();
  Settings s_{};
  WifiCreds wifi_{};
  bool dirty_ = false;
  uint32_t dirtySince_ = 0;
};

extern SettingsManager AppCfg;
