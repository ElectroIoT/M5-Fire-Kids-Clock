// Optional weather from Open-Meteo (free, no API key). Runs in its own
// low-priority FreeRTOS task so a slow HTTP request never freezes the clock.
// The clock, alarms and routine never depend on it.
#pragma once
#include <Arduino.h>

class WeatherManager {
 public:
  // Icon kinds understood by draw::iconWeather()
  enum Kind : uint8_t { UNKNOWN = 0, CLEAR, PARTLY, CLOUDY, FOG, DRIZZLE, RAIN, SNOW, THUNDER };

  void begin();
  void refreshSoon();  // e.g. after the location changed
  bool get(uint8_t& kind, int& tempC, bool& isDay) const;  // false if nothing fresh
  uint32_t ageSec() const;
  const char* lastError() const { return err_; }

  static uint8_t kindFromWmo(int code);

 private:
  static void taskFn(void* arg);
  void run();
  bool fetch();

  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  bool valid_ = false;
  uint8_t kind_ = UNKNOWN;
  int16_t temp_ = 0;
  bool day_ = true;
  uint32_t gotMs_ = 0;
  volatile bool forceRefresh_ = true;
  char err_[40] = "not fetched yet";
  TaskHandle_t task_ = nullptr;
};

extern WeatherManager Weather;
