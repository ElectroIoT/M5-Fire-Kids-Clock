// M5 Fire Kids Clock - compile-time configuration.
// Runtime settings (volume, alarms, Wi-Fi, ...) live in NVS and are edited on the
// device or via the parent web setup page; see SettingsManager.
#pragma once
#include <stdint.h>

#define KC_APP_NAME     "M5 Fire Kids Clock"
#define KC_FW_VERSION   "1.1.0"

// ---- Time --------------------------------------------------------------------
// POSIX TZ for India: IST, UTC+05:30, no daylight saving.
// (POSIX sign is inverted: "IST-5:30" means local = UTC + 5:30.)
#define KC_TZ_POSIX     "IST-5:30"
#define KC_NTP_PRIMARY  "pool.ntp.org"
#define KC_NTP_SECOND   "time.google.com"
// Periodic SNTP re-sync while Wi-Fi is up.
constexpr uint32_t KC_NTP_RESYNC_MS = 3UL * 60UL * 60UL * 1000UL;  // 3 hours
// A clock earlier than this is treated as "not set".
constexpr int KC_MIN_VALID_YEAR = 2025;

// ---- Limits ------------------------------------------------------------------
constexpr int KC_MAX_ALARMS       = 10;
constexpr int KC_MAX_TASKS        = 18;
constexpr int KC_LABEL_LEN        = 24;   // incl. terminator
constexpr int KC_TASK_NAME_LEN    = 24;

// ---- Alarm engine -------------------------------------------------------------
// If the clock jumps forward (NTP correction, manual set) by no more than this,
// alarms inside the skipped minutes still ring. Larger jumps are not replayed.
constexpr int      KC_ALARM_CATCHUP_MIN   = 3;
constexpr uint32_t KC_ALARM_RING_MAX_MS   = 5UL * 60UL * 1000UL;  // auto-snooze after 5 min
constexpr int      KC_ALARM_MAX_AUTOSNOOZE = 3;                   // then stop for good
constexpr uint32_t KC_ALARM_RAMP_MS       = 30UL * 1000UL;        // gradual volume ramp

// ---- Wi-Fi --------------------------------------------------------------------
constexpr uint32_t KC_WIFI_CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t KC_WIFI_RETRY_MIN_MS       = 15000;
constexpr uint32_t KC_WIFI_RETRY_MAX_MS       = 5UL * 60UL * 1000UL;
#define KC_HOSTNAME   "kidsclock"
// 1 = Wi-Fi modem sleep (saves ~30-70 mA, but the app's first request after a
// pause can take seconds). 0 = always awake (default, USB-powered clock).
#ifndef KC_WIFI_POWER_SAVE
  #define KC_WIFI_POWER_SAVE 0
#endif
#define KC_AP_PREFIX  "KidsClock-"
constexpr uint32_t KC_PORTAL_IDLE_TIMEOUT_MS  = 15UL * 60UL * 1000UL;

// Optional compile-time default Wi-Fi (only used when NVS has no network yet).
// Prefer the on-device "Parent Setup" web page instead of editing these.
// Create src/config/Secrets.h with:  #define KC_DEFAULT_SSID "..."  #define KC_DEFAULT_PASS "..."
#if __has_include("Secrets.h")
  #include "Secrets.h"
#endif
#ifndef KC_DEFAULT_SSID
  #define KC_DEFAULT_SSID ""
#endif
#ifndef KC_DEFAULT_PASS
  #define KC_DEFAULT_PASS ""
#endif

// ---- Weather (Open-Meteo, no API key needed) --------------------------------
#define KC_WEATHER_HOST "api.open-meteo.com"
constexpr uint32_t KC_WEATHER_INTERVAL_MS = 30UL * 60UL * 1000UL;
constexpr uint32_t KC_WEATHER_RETRY_MS    = 5UL * 60UL * 1000UL;

// ---- Speaker -------------------------------------------------------------------
// Loudest M5Unified master volume used for volume 10. Measured on the test Fire with
// the M5GO base, powered from a PC USB port: 160+ reset the ESP32 within seconds
// (brownout), 128 once in 3 min, 104 once in 5 min, 88 never (5 min alarm, API test
// with sounds). With a strong 5 V / 2 A adapter it can probably be raised.
// Volumes 1..10 are spread evenly up to this value.
#ifndef KC_SPEAKER_MAX
  #define KC_SPEAKER_MAX 88
#endif

// ---- Display -----------------------------------------------------------------
constexpr int      KC_SCREEN_W        = 320;
constexpr int      KC_SCREEN_H        = 240;
constexpr uint32_t KC_FRAME_MS_ANIM   = 90;    // ~11 fps on animated screens
constexpr uint32_t KC_FRAME_MS_NIGHT  = 1000;  // night screen redraws once a second
constexpr uint32_t KC_MENU_TIMEOUT_MS = 30000; // back to home after inactivity

// ---- Optional external RTC (future hardware expansion) ---------------------
// 0 = none (default), 1 = BM8563/PCF8563 (M5 Unit RTC, 0x51, Port A),
// 2 = DS3231 (0x68). WARNING: DS3231 shares address 0x68 with the Fire's
// MPU6886 on the internal bus - see docs/OFFLINE_AND_RTC.md before enabling.
#ifndef KC_EXTERNAL_RTC
  #define KC_EXTERNAL_RTC 0
#endif
#ifndef KC_RTC_SDA
  #define KC_RTC_SDA 21
#endif
#ifndef KC_RTC_SCL
  #define KC_RTC_SCL 22
#endif

// ---- LEDs ----------------------------------------------------------------------
// The Fire docs suggest pinMode(15, OUTPUT_OPEN_DRAIN) for the LED bar, but the
// RMT driver re-applies push-pull on every frame (as M5Unified's own LED driver
// does on purpose), so the data line runs push-pull. See docs/HARDWARE.md.

// ---- Debug ---------------------------------------------------------------------
constexpr uint32_t KC_SERIAL_BAUD = 115200;
#define KC_LOG(fmt, ...) Serial.printf("[%8lu] " fmt "\n", (unsigned long)millis(), ##__VA_ARGS__)
