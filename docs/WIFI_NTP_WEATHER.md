# Wi-Fi, time sync (NTP) and weather

Wi-Fi is **optional**. The clock, alarms, night mode and routine all run from the ESP32's
own clock without it.

## Setting up Wi-Fi

1. On the clock: **Settings > Parent setup (phone)**.
2. Scan the QR code (or join `KidsClock-XXXX` with the 8-digit password on screen).
3. Open `http://192.168.4.1` (most phones open it automatically).
4. *Wi-Fi* tab: pick the network, type the password, optionally a **backup network**.
   Save - the clock connects immediately.

The page closes itself after 15 minutes without use, or when you press Done / Back.
Each time it opens it uses a new random password. While the clock is on your home
network the same pages are also reachable at its home IP (shown on the setup screen),
but only while the setup screen is open.

Alternatives: serial console `wifi SSID PASSWORD` and `wifi2 SSID PASSWORD` (backup), or
a git-ignored `src/config/Secrets.h` (copy `Secrets.example.h`) used only until a network
is saved.

## Connection behaviour

- Non-blocking state machine (`WiFiLink`): connect with a 20 s timeout; if the main
  network fails the backup is tried straight away; then retries back off 15 s -> 30 s ->
  ... -> 5 min. Lost connection: first retry after ~5 s.
- `WiFi.persistent(false)`: credentials are kept only in the clock's own versioned NVS
  record, not in the Wi-Fi driver's storage.
- *Settings > Wi-Fi* turns Wi-Fi off completely; *Wi-Fi off at night* pauses it inside
  the night window (alarms are unaffected).

## Time synchronisation

| Item | Value |
|---|---|
| Time zone | Asia/Kolkata, POSIX `IST-5:30` (UTC+05:30, no DST) |
| Servers | `pool.ntp.org` (primary), `time.google.com` (secondary) |
| When | every time Wi-Fi connects, then every 3 h, plus *Settings > Sync time now* |
| Validity | anything before 2025 is "not set" |

- An SNTP reply with an implausible date is rejected and the previous good time restored
  ("never overwrite a valid clock with an invalid time").
- The home screen sync icon is filled when the time came from NTP, grey otherwise, with a
  red dot when the time has never been checked online (can be disabled: *Warn if not
  synced*).
- Manual setting: *Settings > Set time & date* (12-hour editor) or the phone page.
- The clock never shows 24-hour time; internally it uses 24-hour values.

## Weather (optional)

- Provider: **Open-Meteo** - free, no account, **no API key**. Request:
  `http://api.open-meteo.com/v1/forecast?latitude=..&longitude=..&current=temperature_2m,weather_code,is_day`
- Configure on the phone page (*Settings* tab): place name, latitude, longitude, on/off.
  Console: `weather 20.30 85.82` / `weather off`.
- Fetched in a separate low-priority FreeRTOS task on core 0 every 30 min (retry 5 min),
  so a slow server never freezes the clock. Data older than 3 h is hidden.
- WMO weather codes map to 8 icons: clear, partly cloudy, cloudy, fog, drizzle, rain,
  snow, thunder (sun or moon depending on day/night).
- No location configured, no Wi-Fi, or server down = simply no weather on screen.
