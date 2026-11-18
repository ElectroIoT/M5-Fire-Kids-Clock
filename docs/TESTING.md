# Testing checklist & results

Test unit: M5Stack Fire K007 (ESP32-D0WDQ6 rev 1.0, 16 MB flash, PSRAM), USB powered,
firmware 1.0.0 built with PlatformIO espressif32 6.9.0 (Arduino-ESP32 2.0.17),
M5Unified 0.3.0, M5GFX 0.3.0, Adafruit NeoPixel 1.15.5. Date of testing: 17 Nov 2026.

Legend: **PASS** = verified on the device · **HOST** = verified by host unit tests ·
**USER** = confirmed by looking at / listening to the device · **NOT TESTED** = needs a check.

## Host unit tests

```bash
g++ -std=c++17 -Wall -Wextra -I../M5_Fire_Kids_Clock/src test_core.cpp ../M5_Fire_Kids_Clock/src/core/TimeLogic.cpp ../M5_Fire_Kids_Clock/src/core/AlarmLogic.cpp -o test_core && ./test_core
```

Run inside `test/` (Linux / WSL / macOS): **113 passed, 0 failed** - 12-hour formatting
for every hour, 12:00 AM / 12:00 PM / 11:59 PM / 12:01 AM, IST from epoch, weekday
transitions at midnight, night windows across midnight, routine day boundary, alarm
matching and next-ring maths for every repeat mode, corrupted-record repair.

## Required checklist

| # | Test | Result | How |
|---|---|---|---|
| 1 | Device starts correctly | PASS | boot log, splash, home screen |
| 2 | Display orientation | PASS / USER | fixed after the first photo (rotation 0), confirmed upright |
| 3 | All three buttons | PASS (partly USER) | navigation via real presses during development + serial-injected presses |
| 4 | Wi-Fi connects | PASS | joined the home network (-68..-73 dBm) |
| 5 | NTP synchronisation | PASS | "NTP sync OK -> 07:21 PM 17 Nov 2026 (IST)" within ~4 s of connecting |
| 6 | 12-hour AM/PM everywhere | PASS / HOST | all screens checked in screenshots; host tests |
| 7 | Indian time zone | PASS | NTP time matched local IST |
| 8 | Time continues after Wi-Fi loss | PASS (design) / HOST | system clock; reconnect state machine exercised with wrong-status bug fixed |
| 9 | Alarms work offline | PASS | alarm engine uses only system time; midnight test ran on a manually set clock |
| 10 | Alarm at the correct minute | PASS | rang at 07:27:00-07:27:01 PM |
| 11 | Snooze | PASS | A -> snoozed 5 min, re-rang as "Snooze is over!" |
| 12 | Stop | PASS | B/C and console `stop` |
| 13 | Repeating alarms | HOST | every repeat mode in unit tests; daily 12:00 AM ring on device |
| 14 | Weekday selection | HOST / PASS | unit tests; editor day picker screenshot |
| 15 | Alarm settings survive reboot | PASS | `reboot`, alarm list identical, one-time alarms saved as off |
| 16 | Audio plays | USER | melody heard on the device |
| 17 | RGB LEDs | USER | rainbow effect seen on the M5GO bar |
| 18 | Night mode | PASS | started automatically at 9 PM window (tested 11:59 PM), dim face, AM/PM shown |
| 19 | Daily routine resets correctly | PASS | reset exactly at 3:00 AM, total stars kept, ticks kept after reboot, backwards NTP step does not wipe ticks |
| 20 | Battery information | PASS (USB only) | 100 %, not charging (IP5306 reports no voltage on the Fire) |

## Time edge cases

| Case | Result |
|---|---|
| 12:00 AM | PASS (device ring) + HOST |
| 12:00 PM | HOST |
| 11:59 PM -> 12:00 AM weekday change | PASS (Tue 17 -> Wed 18 Nov) + HOST |
| 12:01 AM | HOST |
| Wi-Fi disconnection | PASS - clock kept running; reconnect with back-off; fixed a bug where a stale `WiFi.status()` aborted every attempt |
| NTP failure / bad reply | design: invalid replies rejected, 30 s timeout logged, retries in background |
| ESP32 reboot | PASS - time retained after software reset, alarms and routine kept |
| Clock set backwards / forwards | PASS - no double rings (fired-minute guard), routine re-anchors |

## Other verified items

- Parent setup screen: access point `KidsClock-XXXX` starts, QR + password shown (PASS).
- Parent web pages over the home network: `/`, `/wifi`, `/alarms`, `/routine`, `/settings`
  all HTTP 200 and complete; alarm saved via the form and restored (PASS). Found and fixed
  three bugs on the way: the idle timer closed the page after the first request (unsigned
  wrap-around), an empty field ended the page early (empty HTTP chunk), and pages were
  sent as hundreds of tiny packets (now buffered: 38 KB page in 0.27 s instead of 7 s).
  Wi-Fi power saving is now off while the page is open.
- Child's name shown in greeting, splash, alarm, reminder and stars screens (PASS, screenshots).
- Weather: Open-Meteo fetch with a test location returned 19 °C, clear night icon (PASS);
  test location removed afterwards.
- Performance: main-loop sections other than the UI take < 1.5 ms; UI frame ~24 ms render
  + ~32 ms SPI push; worst loop pass ~95 ms when the static layer is rebuilt (every 5 s).
  Heap free ~206 KB, PSRAM free ~3.8 MB.

## Phase B - local API and power (18 Nov 2026)

| Test | Result |
|---|---|
| `tools/api_test.py` (pairing, 401/403/409, alarms, routine + events, settings, latency) | **21/21 PASS**, also at volume 10 |
| API latency, Wi-Fi modem sleep on | first request after a pause 4 s or timeout - modem sleep now off by default |
| API latency, modem sleep off | 16-144 ms, median ~70 ms; ping 3-13 ms |
| 60 back-to-back requests | 60/60 OK, heap min 167 KB, no restart |
| mDNS `kidsclock.local` / `_kidsclock._tcp` | announced (Windows resolution not separately verified) |
| Alarm at old full volume, base off | brownout restart within 20-40 s (even at volume 6) |
| Alarm at old volume 10, base on | 3 restarts in 3 min; alarm resumed each time |
| Alarm at master 136 / 160 / 128 / 104 / 88, base on, PC USB port | 0 dips in 90 s / dips / 1 dip in 3 min / 1 dip in 5 min / **0 dips in 5 min** -> ceiling 88 |
| Alarm resume after brownout | resumed in 2 of 3 observed restarts (1 unexplained miss) |

## NOT TESTED on hardware - please check

- **Parent setup pages from a phone over the clock's own access point** (`KidsClock-XXXX`,
  `http://192.168.4.1`). Over the home network all five pages load fully (0.03-0.3 s) and
  saving an alarm through the form works - see below.
- microSD WAV playback (no card inserted).
- Running on battery only; low-battery warning; charging indicator.
- External RTC driver (no RTC unit).
- Bedtime lullaby at the real night-mode start, sunrise / morning LED moods at the real
  night-mode end, Wi-Fi-off-at-night.
- Long-term stability (multi-day run), drift without Wi-Fi.
- **Arduino IDE / Arduino-ESP32 core 3.x build** - only core 2.0.17 (PlatformIO) was compiled.

## Screenshots

`python tools/kc.py COM16 shot file.png` - see `docs/screenshots/`.
