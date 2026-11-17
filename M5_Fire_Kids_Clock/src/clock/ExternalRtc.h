// Optional external RTC for future hardware expansion (disabled by default).
//
//   KC_EXTERNAL_RTC 0  -> no RTC (default build; the clock runs from ESP32 system time)
//   KC_EXTERNAL_RTC 1  -> BM8563 / PCF8563 at 0x51 (M5Stack Unit RTC) on Port A
//   KC_EXTERNAL_RTC 2  -> DS3231 at 0x68 on Port A
//
// The Fire's internal I2C bus IS Port A (GPIO21/22). The MPU6886 IMU on most
// Fire units also sits at 0x68, so a DS3231 collides with it. Use the BM8563
// unit, or a DS3231 only on an SH200Q-revision Fire. See docs/OFFLINE_AND_RTC.md.
#pragma once
#include <time.h>

namespace extrtc {
bool begin();                 // true if an RTC answered
bool present();
bool read(time_t& utcEpoch);  // false if absent or its time is invalid / lost power
bool write(time_t utcEpoch);
const char* name();
}  // namespace extrtc
