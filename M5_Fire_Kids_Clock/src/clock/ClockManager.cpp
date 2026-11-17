#include "ClockManager.h"
#include <esp_sntp.h>
#include <sys/time.h>
#include "../config/Config.h"
#include "../core/TimeLogic.h"
#include "ExternalRtc.h"

ClockManager Clock;

// Survives software resets / watchdog resets (not power loss). Lets the clock
// remember that its time came from NTP across a crash-reboot.
RTC_NOINIT_ATTR static uint32_t g_syncMagic;
RTC_NOINIT_ATTR static uint32_t g_syncEpoch;
static constexpr uint32_t kSyncMagic = 0x5EC0FFEE;

static void sntpCallback(struct timeval* tv) { Clock.handleSyncEvent(tv); }

bool ClockManager::epochValid(time_t t) const {
  // 2025-01-01 00:00 UTC
  static const time_t kMin = static_cast<time_t>(tl::daysFromCivil(KC_MIN_VALID_YEAR, 1, 1)) * 86400;
  return t >= kMin && t < kMin + 100LL * 365 * 86400;
}

bool ClockManager::isValid() const { return epochValid(time(nullptr)); }

bool ClockManager::everSynced() const { return g_syncMagic == kSyncMagic; }

void ClockManager::begin() {
  setenv("TZ", KC_TZ_POSIX, 1);
  tzset();

  const esp_reset_reason_t rr = esp_reset_reason();
  if (rr == ESP_RST_POWERON || rr == ESP_RST_BROWNOUT || rr == ESP_RST_UNKNOWN) {
    g_syncMagic = 0;  // RTC_NOINIT content is garbage after power-on
    g_syncEpoch = 0;
  }

  if (isValid()) {
    // Software reset: the ESP32 keeps system time across esp_restart()/WDT resets.
    source_ = everSynced() ? TimeSource::Ntp : TimeSource::Retained;
    lastSync_ = everSynced() ? static_cast<time_t>(g_syncEpoch) : 0;
    KC_LOG("Clock: time retained across reset (%s)", sourceName());
  }

  if (extrtc::begin() && !isValid()) {
    time_t t;
    if (extrtc::read(t)) {
      struct timeval tv{t, 0};
      settimeofday(&tv, nullptr);
      source_ = TimeSource::Rtc;
      ++steps_;
      KC_LOG("Clock: restored from %s", extrtc::name());
    }
  }

  if (isValid()) {
    lastGoodEpoch_ = time(nullptr);
    lastGoodMs_ = millis();
  } else {
    KC_LOG("Clock: time not set yet - waiting for NTP or manual set");
  }
  sntp_set_time_sync_notification_cb(sntpCallback);
}

void ClockManager::startSntp() {
  sntp_set_sync_interval(KC_NTP_RESYNC_MS);
  // configTzTime() (re)starts SNTP and re-applies the IST TZ string.
  configTzTime(KC_TZ_POSIX, KC_NTP_PRIMARY, KC_NTP_SECOND);
  sntpStarted_ = true;
  syncing_ = true;
  lastSyncMs_ = millis();
  KC_LOG("Clock: SNTP started (%s, %s)", KC_NTP_PRIMARY, KC_NTP_SECOND);
}

void ClockManager::onNetworkUp() {
  online_ = true;
  startSntp();
}

void ClockManager::onNetworkDown() {
  online_ = false;
  syncing_ = false;
  // SNTP keeps running and simply retries; system time keeps counting offline.
}

void ClockManager::requestSync() {
  if (online_) startSntp();
}

// Runs in the lwIP task: keep it short, only validate and flag.
void ClockManager::handleSyncEvent(const struct timeval* tv) {
  if (!tv || !epochValid(tv->tv_sec)) {
    // Never let a bogus server reply replace a good clock.
    if (lastGoodEpoch_ != 0) {
      struct timeval fix{static_cast<time_t>(lastGoodEpoch_ + (millis() - lastGoodMs_) / 1000), 0};
      settimeofday(&fix, nullptr);
    }
    return;
  }
  syncFlag_ = true;
}

void ClockManager::loop() {
  const uint32_t ms = millis();
  if (syncFlag_) {
    syncFlag_ = false;
    syncing_ = false;
    source_ = TimeSource::Ntp;
    lastSync_ = time(nullptr);
    g_syncMagic = kSyncMagic;
    g_syncEpoch = static_cast<uint32_t>(lastSync_);
    ++steps_;
    LocalTime t = now();
    char hm[12];
    tl::formatHM12AmPm(hm, sizeof hm, t.hour, t.minute);
    KC_LOG("Clock: NTP sync OK -> %s %02d %s %d (IST)", hm, t.day, tl::monthShort(t.month), t.year);
    if (extrtc::present() && extrtc::write(lastSync_)) KC_LOG("Clock: %s updated", extrtc::name());
  }
  if (syncing_ && ms - lastSyncMs_ > 30000) {
    syncing_ = false;
    KC_LOG("Clock: NTP sync timed out (will keep retrying in background)");
  }
  // Remember the last known-good time once a minute (used to undo bad steps).
  if (isValid() && ms - lastGoodMs_ > 60000) {
    lastGoodEpoch_ = time(nullptr);
    lastGoodMs_ = ms;
  }
  // Keep an external RTC disciplined every 6 hours.
  if (extrtc::present() && isValid() && ms - lastRtcWriteMs_ > 6UL * 3600UL * 1000UL) {
    lastRtcWriteMs_ = ms;
    if (source_ == TimeSource::Ntp || source_ == TimeSource::Manual) extrtc::write(time(nullptr));
  }
}

LocalTime ClockManager::now() const {
  LocalTime lt{};
  const time_t t = time(nullptr);
  lt.valid = epochValid(t);
  struct tm tmv;
  localtime_r(&t, &tmv);
  lt.year = tmv.tm_year + 1900;
  lt.month = tmv.tm_mon + 1;
  lt.day = tmv.tm_mday;
  lt.hour = tmv.tm_hour;
  lt.minute = tmv.tm_min;
  lt.second = tmv.tm_sec;
  lt.wday = tmv.tm_wday;
  return lt;
}

bool ClockManager::setManual(int year, int month, int day, int hour24, int minute) {
  if (year < KC_MIN_VALID_YEAR || year > 2099 || month < 1 || month > 12 || day < 1 ||
      day > tl::daysInMonth(year, month) || hour24 < 0 || hour24 > 23 || minute < 0 || minute > 59) {
    return false;
  }
  const time_t t = static_cast<time_t>(tl::daysFromCivil(year, month, day)) * 86400 + hour24 * 3600 + minute * 60 -
                   tl::IST_OFFSET_MIN * 60;
  if (!epochValid(t)) return false;
  struct timeval tv{t, 0};
  settimeofday(&tv, nullptr);
  source_ = TimeSource::Manual;
  lastGoodEpoch_ = t;
  lastGoodMs_ = millis();
  ++steps_;
  if (extrtc::present()) extrtc::write(t);
  char hm[12];
  tl::formatHM12AmPm(hm, sizeof hm, hour24, minute);
  KC_LOG("Clock: set manually to %02d %s %d %s", day, tl::monthShort(month), year, hm);
  return true;
}

const char* ClockManager::sourceName() const {
  switch (source_) {
    case TimeSource::Ntp: return "NTP";
    case TimeSource::Manual: return "manual";
    case TimeSource::Rtc: return "external RTC";
    case TimeSource::Retained: return "kept after reset";
    default: return "not set";
  }
}

void ClockManager::printStatus(Print& out) const {
  LocalTime t = now();
  char hm[12], date[40];
  tl::formatHM12AmPm(hm, sizeof hm, t.hour, t.minute);
  tl::formatDateLong(date, sizeof date, t.wday, t.year, t.month, t.day);
  out.printf("Time   : %s :%02d  %s  (IST, valid=%d)\n", hm, t.second, date, t.valid);
  out.printf("Source : %s, synced since boot=%d, syncing=%d\n", sourceName(), everSynced(), syncing_);
  if (lastSync_) {
    struct tm s;
    localtime_r(&lastSync_, &s);
    tl::formatHM12AmPm(hm, sizeof hm, s.tm_hour, s.tm_min);
    out.printf("LastNTP: %s %02d %s\n", hm, s.tm_mday, tl::monthShort(s.tm_mon + 1));
  }
}
