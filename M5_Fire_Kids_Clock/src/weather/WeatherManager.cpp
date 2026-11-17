#include "WeatherManager.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <math.h>
#include "../config/Config.h"
#include "../settings/SettingsManager.h"
#include "../wifi/WiFiLink.h"

WeatherManager Weather;

static constexpr uint32_t kStaleMs = 3UL * 60UL * 60UL * 1000UL;  // hide after 3 h without update

uint8_t WeatherManager::kindFromWmo(int c) {
  if (c == 0) return CLEAR;
  if (c == 1 || c == 2) return PARTLY;
  if (c == 3) return CLOUDY;
  if (c == 45 || c == 48) return FOG;
  if (c >= 51 && c <= 57) return DRIZZLE;
  if ((c >= 61 && c <= 67) || (c >= 80 && c <= 82)) return RAIN;
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return SNOW;
  if (c >= 95 && c <= 99) return THUNDER;
  return UNKNOWN;
}

void WeatherManager::begin() {
  // Core 0, priority 1: below the Wi-Fi/lwIP tasks, away from the UI loop (core 1).
  xTaskCreatePinnedToCore(taskFn, "weather", 6144, this, 1, &task_, 0);
}

void WeatherManager::refreshSoon() { forceRefresh_ = true; }

bool WeatherManager::get(uint8_t& kind, int& tempC, bool& isDay) const {
  portENTER_CRITICAL(&mux_);
  const bool ok = valid_ && (millis() - gotMs_ < kStaleMs);
  kind = kind_;
  tempC = temp_;
  isDay = day_;
  portEXIT_CRITICAL(&mux_);
  return ok && AppCfg.get().weatherEnabled;
}

uint32_t WeatherManager::ageSec() const { return valid_ ? (millis() - gotMs_) / 1000 : 0; }

void WeatherManager::taskFn(void* arg) { static_cast<WeatherManager*>(arg)->run(); }

void WeatherManager::run() {
  uint32_t lastTry = 0;
  bool lastOk = false;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(2000));
    const Settings& s = AppCfg.get();
    if (!s.weatherEnabled || isnan(s.latitude) || isnan(s.longitude) || !Net.connected()) continue;
    const uint32_t wait = lastOk ? KC_WEATHER_INTERVAL_MS : KC_WEATHER_RETRY_MS;
    if (!forceRefresh_ && lastTry && millis() - lastTry < wait) continue;
    forceRefresh_ = false;
    lastTry = millis();
    lastOk = fetch();
  }
}

// Minimal parser: read a number after "key": inside the "current":{...} object.
static bool readNumber(const String& body, int from, const char* key, float& out) {
  const int k = body.indexOf(key, from);
  if (k < 0) return false;
  const int colon = body.indexOf(':', k);
  if (colon < 0) return false;
  out = body.substring(colon + 1, colon + 16).toFloat();
  return true;
}

bool WeatherManager::fetch() {
  const Settings& s = AppCfg.get();
  char url[200];
  snprintf(url, sizeof url,
           "http://" KC_WEATHER_HOST "/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,weather_code,is_day&timezone=auto",
           s.latitude, s.longitude);
  HTTPClient http;
  http.setTimeout(8000);
  http.setConnectTimeout(5000);
  if (!http.begin(url)) {
    strlcpy(err_, "bad URL", sizeof err_);
    return false;
  }
  const int code = http.GET();
  if (code != 200) {
    snprintf(err_, sizeof err_, "HTTP %d", code);
    http.end();
    KC_LOG("Weather: %s", err_);
    return false;
  }
  const String body = http.getString();
  http.end();
  const int cur = body.indexOf("\"current\":{");
  float t = NAN, w = -1, d = 1;
  if (cur < 0 || !readNumber(body, cur, "\"temperature_2m\"", t) || !readNumber(body, cur, "\"weather_code\"", w)) {
    strlcpy(err_, "unexpected reply", sizeof err_);
    KC_LOG("Weather: %s", err_);
    return false;
  }
  readNumber(body, cur, "\"is_day\"", d);
  portENTER_CRITICAL(&mux_);
  temp_ = (int16_t)lroundf(t);
  kind_ = kindFromWmo((int)w);
  day_ = d >= 0.5f;
  valid_ = true;
  gotMs_ = millis();
  portEXIT_CRITICAL(&mux_);
  strlcpy(err_, "ok", sizeof err_);
  KC_LOG("Weather: %d C, code %d (%s)", (int)lroundf(t), (int)w, d >= 0.5f ? "day" : "night");
  return true;
}
