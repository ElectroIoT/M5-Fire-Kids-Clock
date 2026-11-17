# Android companion app - plan

Status: **plan only** (nothing built yet). Decided: daily use over **home Wi-Fi**,
**Bluetooth LE only for first-time setup**.

## Goals

1. Set up a new clock without joining its temporary Wi-Fi (BLE provisioning).
2. Edit alarms, routine jobs, child's name, night mode and weather with proper phone UI.
3. See today's progress and a star history; get a phone notification when the morning
   (or any) routine is finished, or an alarm was not stopped.
4. Everything stays on the home network - **no cloud account, no child data leaves the house**.

Non-goals for v1: remote access from outside the home, multiple clocks per family
(designed for, not built), iOS.

## How the parts talk

```
 first setup                       daily use (same Wi-Fi)
 ┌──────────┐   BLE (provisioning)  ┌─────────────┐   HTTP + JSON   ┌──────────┐
 │  Phone   │ ───────────────────▶ │ Fire clock  │ ◀────────────── │  Phone   │
 │ (app)    │  Wi-Fi creds + PoP    │ ESP32       │  mDNS discovery │ (app)    │
 └──────────┘                       └─────────────┘  bearer token   └──────────┘
```

## Firmware work (clock side)

### F1. Local JSON API (always on while connected to home Wi-Fi)

- mDNS: `kidsclock.local`, service `_kidsclock._tcp` (TXT: id, fw, name-free).
- Auth: pairing. *Settings > Pair phone* shows a 6-digit code for 2 minutes; app sends
  `POST /api/v1/pair {code}` and receives a random 128-bit token (up to 4 phones, stored in
  NVS, revocable on the clock). Every other request needs `Authorization: Bearer <token>`.
  No token = HTTP 401. The existing parent web page stays as the no-app fallback.
- Endpoints (JSON; all times as `{"hour12":7,"minute":30,"pm":false}` + 24 h value):

| Method & path | Purpose |
|---|---|
| `GET /api/v1/status` | time, sync source, Wi-Fi RSSI, battery, night mode, ringing, fw version |
| `GET/PUT /api/v1/alarms`, `PUT /api/v1/alarms/{n}` | read / write alarms (same fields as NVS) |
| `POST /api/v1/alarms/{n}/test`, `POST /api/v1/alarm/stop` | ring test / stop |
| `GET/PUT /api/v1/routine`, `POST /api/v1/routine/{n}/done` | jobs, ticks |
| `GET /api/v1/stars?days=30` | daily star history (new: 30-day ring buffer in NVS) |
| `GET/PUT /api/v1/settings` | volume, brightness, night window, LEDs, weather place, child's name |
| `POST /api/v1/time` | set time (when no internet) |
| `GET /api/v1/events?after=SEQ` | event log for notifications (routine done, alarm stopped / ignored, battery low) |

- Implementation notes: reuse the existing `WebServer` (already serves the parent page),
  add ArduinoJson (~20 KB flash), keep handlers non-blocking, rate-limit pairing attempts.

### F2. BLE provisioning (setup only)

- Preferred: Espressif's standard **Wi-Fi provisioning** protocol (`WiFiProv` in
  Arduino-ESP32, security 1 with a proof-of-possession code shown on the clock screen),
  so the app can use Espressif's maintained Android provisioning library.
- Alternative if RAM is tight: a minimal custom GATT service on **NimBLE-Arduino**.
- BLE is started only when no Wi-Fi is saved, or from *Settings > Pair phone (Bluetooth)*,
  and is shut down after success or 10 minutes, so it never competes with Wi-Fi at night.
- **Spike first:** measure free heap with Wi-Fi + web server + BLE on the Fire (today ~206 KB
  free; Bluedroid needs roughly 100 KB, NimBLE roughly 50 KB). Decide WiFiProv vs NimBLE
  from the measurement.

### F3. Events & history

- Small event ring buffer in RAM (+ last 50 in NVS) with a sequence number.
- Daily star totals for 30 days (one `uint16` per day).

## App (phone side)

| Area | Choice |
|---|---|
| Language / UI | Kotlin, Jetpack Compose, Material 3 with the clock's pastel palette |
| Architecture | MVVM + repository, Kotlin coroutines / Flow, Hilt |
| Network | OkHttp + Retrofit + kotlinx.serialization; `NsdManager` for mDNS discovery |
| BLE setup | Espressif `esp-idf-provisioning-android` (or Android BLE API for the NimBLE variant) |
| Storage | DataStore (paired clocks + tokens, encrypted with Android Keystore) |
| Background | WorkManager: check `/events` every 15 min while on the home Wi-Fi; notifications |
| Min SDK | 26 (Android 8), target latest |

### Screens

1. **Welcome / add clock** - BLE scan, pick "KidsClock-XXXX", enter PoP shown on the clock,
   choose Wi-Fi + password, then pairing code -> done.
2. **Today** - big clock status (time synced?, battery, Wi-Fi), next alarm, child's progress
   ring per part of the day, stars today, "Ring test" and "Stop alarm" buttons.
3. **Alarms** - list with switches; editor with a 12-hour wheel picker + AM/PM, repeat chips,
   sound preview (plays on the clock), name.
4. **Routine** - jobs per part of the day, drag to reorder, time + reminder toggle, add / delete.
5. **Stars** - weekly bar chart, badges, reset (with confirm).
6. **Settings** - child's name, night mode window, volume, brightness, LEDs, weather place
   (with "use my phone's location" to fill latitude / longitude), quiet mode.
7. **Device** - firmware version, IP, battery, reboot, unpair, factory reset.

### Notifications (opt-in, per type)

"<name> finished the morning jobs!", "Wake-up alarm rang for 5 min without being stopped",
"Clock battery low", "Clock lost its time - check Wi-Fi". Delivered only when the phone is
on the same network (no cloud). A later opt-in could use a self-hosted push (e.g. ntfy).

## Phases

| # | Deliverable | Done when |
|---|---|---|
| A | BLE memory spike on the Fire | heap numbers measured, provisioning stack chosen |
| B | Firmware API + mDNS + pairing | `curl` with token reads/writes alarms; 401 without token; pages still work |
| C | App skeleton: discovery, pairing, Today screen | phone finds `kidsclock.local`, pairs, shows status |
| D | Alarms + Routine editors | edits round-trip and survive a clock reboot |
| E | Settings, Stars history, Device | all clock settings editable from the app |
| F | BLE provisioning flow | fresh clock set up from the app without touching its Wi-Fi |
| G | Notifications (WorkManager + events) | routine-done notification arrives within 15 min |
| H | Polish, icon, Play listing | closed testing on Play Console |

Before naming: check the Play Store and the web for collisions with the chosen app name.

## Risks / open questions

- **Wi-Fi + BLE coexistence** on one ESP32 radio: limited by enabling BLE only during setup.
- **RAM**: Bluedroid may be too big next to Wi-Fi + web server + 150 KB sprites (sprites are
  in PSRAM, so likely fine) - phase A answers this.
- **Android local-network rules**: recent Android versions restrict mDNS / local traffic in
  some cases; fall back to entering the IP shown on the clock.
- **Arduino-ESP32 3.x**: firmware is verified on 2.0.17 only; the API work should also
  add a core-3 CI build.
- Security: LAN-only HTTP with bearer tokens; HTTPS on the ESP32 is possible but heavy.
  Pairing code + rate limit + token revocation is the v1 trade-off.
