#include "ExternalRtc.h"
#include <M5Unified.h>
#include "../config/Config.h"
#include "../core/TimeLogic.h"

namespace extrtc {

static bool s_present = false;

#if KC_EXTERNAL_RTC == 1
static constexpr uint8_t kAddr = 0x51;
static constexpr const char* kName = "BM8563";
#elif KC_EXTERNAL_RTC == 2
static constexpr uint8_t kAddr = 0x68;
static constexpr const char* kName = "DS3231";
#else
static constexpr uint8_t kAddr = 0;
static constexpr const char* kName = "none";
#endif
static constexpr uint32_t kFreq = 100000;

static uint8_t bcd2bin(uint8_t v) { return (v & 0x0F) + 10 * (v >> 4); }
static uint8_t bin2bcd(uint8_t v) { return static_cast<uint8_t>(((v / 10) << 4) | (v % 10)); }

const char* name() { return kName; }
bool present() { return s_present; }

bool begin() {
#if KC_EXTERNAL_RTC == 0
  return false;
#else
  s_present = M5.In_I2C.scanID(kAddr, kFreq);
#if KC_EXTERNAL_RTC == 1
  if (s_present) {
    const uint8_t init[2] = {0x00, 0x00};  // control/status 1+2: normal mode, no interrupts
    M5.In_I2C.writeRegister(kAddr, 0x00, init, 2, kFreq);
  }
#endif
  KC_LOG("External RTC %s: %s", kName, s_present ? "found" : "not found");
  return s_present;
#endif
}

// The RTC stores IST wall-clock time (what a human would set on it).
bool read(time_t& utcEpoch) {
  if (!s_present) return false;
  uint8_t r[7];
#if KC_EXTERNAL_RTC == 1
  if (!M5.In_I2C.readRegister(kAddr, 0x02, r, 7, kFreq)) return false;
  if (r[0] & 0x80) return false;  // VL flag: oscillator stopped, time is not reliable
  const int sec = bcd2bin(r[0] & 0x7F), min = bcd2bin(r[1] & 0x7F), hour = bcd2bin(r[2] & 0x3F);
  const int day = bcd2bin(r[3] & 0x3F), mon = bcd2bin(r[5] & 0x1F), year = 2000 + bcd2bin(r[6]);
#elif KC_EXTERNAL_RTC == 2
  if (!M5.In_I2C.readRegister(kAddr, 0x00, r, 7, kFreq)) return false;
  uint8_t status = 0;
  if (M5.In_I2C.readRegister(kAddr, 0x0F, &status, 1, kFreq) && (status & 0x80)) return false;  // OSF
  const int sec = bcd2bin(r[0] & 0x7F), min = bcd2bin(r[1] & 0x7F), hour = bcd2bin(r[2] & 0x3F);
  const int day = bcd2bin(r[4] & 0x3F), mon = bcd2bin(r[5] & 0x1F), year = 2000 + bcd2bin(r[6]);
#else
  const int sec = 0, min = 0, hour = 0, day = 1, mon = 1, year = 2000;
  (void)r;
#endif
  if (year < KC_MIN_VALID_YEAR || mon < 1 || mon > 12 || day < 1 || day > 31 || hour > 23 || min > 59 || sec > 59) return false;
  utcEpoch = static_cast<time_t>(tl::daysFromCivil(year, mon, day)) * 86400 + hour * 3600 + min * 60 + sec -
             tl::IST_OFFSET_MIN * 60;
  return true;
}

bool write(time_t utcEpoch) {
  if (!s_present) return false;
  const int64_t local = static_cast<int64_t>(utcEpoch) + tl::IST_OFFSET_MIN * 60;
  const auto p = tl::localFromEpochMinute(local / 60, 0);
  // civil from days (inverse of daysFromCivil)
  int32_t z = p.dayNumber + 719468;
  const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  const uint32_t doe = static_cast<uint32_t>(z - era * 146097);
  const uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int y = static_cast<int>(yoe) + era * 400;
  const uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const uint32_t mp = (5 * doy + 2) / 153;
  const int d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  const int m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  y += (m <= 2);
  const int sec = static_cast<int>(local % 60);
  uint8_t w[7];
#if KC_EXTERNAL_RTC == 1
  w[0] = bin2bcd(sec); w[1] = bin2bcd(p.minute); w[2] = bin2bcd(p.hour); w[3] = bin2bcd(d);
  w[4] = static_cast<uint8_t>(p.wday); w[5] = bin2bcd(m); w[6] = bin2bcd(y % 100);
  return M5.In_I2C.writeRegister(kAddr, 0x02, w, 7, kFreq);
#elif KC_EXTERNAL_RTC == 2
  w[0] = bin2bcd(sec); w[1] = bin2bcd(p.minute); w[2] = bin2bcd(p.hour); w[3] = static_cast<uint8_t>(p.wday + 1);
  w[4] = bin2bcd(d); w[5] = bin2bcd(m); w[6] = bin2bcd(y % 100);
  bool ok = M5.In_I2C.writeRegister(kAddr, 0x00, w, 7, kFreq);
  uint8_t status = 0;
  if (ok && M5.In_I2C.readRegister(kAddr, 0x0F, &status, 1, kFreq)) {
    status &= 0x7F;  // clear OSF
    M5.In_I2C.writeRegister(kAddr, 0x0F, &status, 1, kFreq);
  }
  return ok;
#else
  (void)w; (void)sec; (void)d; (void)m; (void)y;
  return false;
#endif
}

}  // namespace extrtc
