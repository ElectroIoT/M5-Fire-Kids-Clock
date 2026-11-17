# Offline operation & the optional external RTC

## What works without internet

Everything except time sync and weather: clock, alarms, snooze, night mode, routine,
stars, reminders, sounds, LEDs, settings. Wi-Fi drop-outs are handled silently and the
clock reconnects in the background.

## How time is kept

| Situation | Result |
|---|---|
| Wi-Fi lost | ESP32 system clock keeps running (drift of a few seconds per day); resyncs on reconnect |
| Software reset / watchdog / firmware crash | time is **kept** (ESP32 RTC timer survives); shown as "kept after reset" until the next NTP sync |
| Firmware upload (USB reset) | usually treated like power-on: time is lost until NTP / manual set |
| **Complete power loss** (USB unplugged and battery empty, or power switched off) | **time is lost**. The clock shows "What time is it?" until NTP succeeds or a parent sets it. Alarms wait until a valid time exists. |

The Fire has **no battery-backed RTC chip**. For a bedside clock on USB power with Wi-Fi
this is fine - NTP fixes the time seconds after boot. Without Wi-Fi, set the time manually
after a power cut (*Settings > Set time & date*).

## Optional external RTC (future expansion)

`src/clock/ExternalRtc.*` contains a small driver, disabled by default. Enable it with a
build flag in `platformio.ini`:

```ini
build_flags = ${env.build_flags} -DKC_EXTERNAL_RTC=1
```

| Value | Chip | Address | Notes |
|---|---|---|---|
| 0 | none | - | default |
| 1 | BM8563 / PCF8563 (M5Stack *Unit RTC*) | 0x51 | **recommended** - plug into Grove Port A |
| 2 | DS3231 | 0x68 | **conflicts with the Fire's MPU6886 IMU (also 0x68)** on the internal bus = Port A. Only use on an SH200Q-revision Fire, or rewire to another bus. |

Behaviour when enabled: at boot, if the system time is not valid, it is restored from the
RTC (if the RTC's oscillator-stopped flag is clear). After each NTP sync or manual set,
and every 6 h, the RTC is updated. The RTC stores IST wall-clock time.

This driver is **not tested on hardware** (no RTC unit was attached during development).
