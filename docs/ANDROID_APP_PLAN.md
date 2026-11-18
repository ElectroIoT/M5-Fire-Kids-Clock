# Android companion app - plan

Status: **phase A done** (18 Nov 2026), app not built yet. Decided: daily use over
**home Wi-Fi**, **Bluetooth LE only for first-time setup**, stack = **NimBLE**.

## Phase A results - Bluetooth on the Fire (measured on the device)

Same firmware with Wi-Fi connected, UI running; BLE = GATT server with 2 characteristics,
advertising. Built in three variants (`pio run -e spike-baseline|spike-bluedroid|spike-nimble`,
console `ble on|off|mem`, code in `src/ble/BleSpike.*`, flag `KC_BLE_SPIKE`).

| | No Bluetooth | Bluedroid | NimBLE |
|---|---|---|---|
| Extra firmware size | - | **+761 KB** | **+240 KB** |
| Internal RAM free, BT linked but off | 166-175 KB | 116-125 KB | 117-126 KB |
| Internal RAM free, BT running | - | **40 KB** (lowest 30 KB) | **105 KB** (lowest 84 KB) |
| Largest free block, BT running | 110 KB | **39 KB** | 102 KB |
| Largest free block after BT off | 110 KB | **43 KB (fragmented)** | 110 KB |
| UI frame (render + push) | ~29 + 32 ms | ~31 + 32 ms | ~38 + 32 ms |

Findings:

1. **NimBLE is the clear choice**: a quarter of the RAM cost, a third of the flash, no
   heap fragmentation. Bluedroid leaves too little contiguous RAM for Wi-Fi + HTTP and
   rules out Espressif's Bluedroid-based `WiFiProv`, so provisioning will be a small custom
   GATT service on NimBLE (the app side uses the plain Android BLE API).
2. **Linking any Bluetooth stack costs ~45 KB of RAM even while BT is off** (the Arduino
   core keeps the controller memory when a BT library is present). Plan: release it with
   `esp_bt_controller_mem_release()` once provisioning is finished for good, or keep BT
   in a separate setup build if RAM becomes tight.
3. **Wi-Fi power saving and BLE:** with BT on, the ESP32 requires Wi-Fi modem sleep.
   The setup page turns modem sleep off (needed for reliable AP+STA pages) and doing that
   with BT running **aborts** (`wifi_set_ps_process -> pm_set_sleep_type`, reproduced).
   With modem sleep forced on, the AP+STA setup pages time out. Rule: **never run BLE and
   the setup access point at the same time**; BLE runs only in its own setup window.
4. Daily API traffic will be station-only Wi-Fi with modem sleep - latency to be measured
   in phase B.

### Side finding: Fire upload / flash quirk (fixed)

Uploads sometimes failed with "Failed to communicate with the flash chip" and the clock
then stopped booting (`flash read err, 1000`) until fully powered off. GPIO12 strapping was
ruled out (always low); the flash chip ignored wake/reset commands. Cause (supported by
the fix): the PSRAM shares data lines with the flash; a reset during a PSRAM transfer can
leave it driving those lines. Fix: console `flashprep` stops all drawing, and a
PlatformIO pre-upload hook (`tools/pio_flashprep.py`) sends it automatically. Result:
7 consecutive uploads without a failure, versus repeated failures before.

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

- **Decided after phase A:** a minimal custom GATT service on **NimBLE-Arduino 1.4.x**
  (Wi-Fi scan list, SSID, password write-only, apply, status notify, pairing token),
  protected by a 6-digit code shown on the clock. Espressif's `WiFiProv` was ruled out
  because it needs Bluedroid (see results above).
- BLE is started only when no Wi-Fi is saved, or from *Settings > Pair phone (Bluetooth)*,
  and is shut down after success or 10 minutes, so it never competes with Wi-Fi at night.
- Never together with the setup access point (see finding 3); free the BT controller
  memory after setup if RAM is needed.

### F3. Events & history

- Small event ring buffer in RAM (+ last 50 in NVS) with a sequence number.
- Daily star totals for 30 days (one `uint16` per day).

## App (phone side)

| Area | Choice |
|---|---|
| Language / UI | Kotlin, Jetpack Compose, Material 3 with the clock's pastel palette |
| Architecture | MVVM + repository, Kotlin coroutines / Flow, Hilt |
| Network | OkHttp + Retrofit + kotlinx.serialization; `NsdManager` for mDNS discovery |
| BLE setup | Android BLE API (custom NimBLE GATT service on the clock) |
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
| A | BLE memory spike on the Fire | **done** - NimBLE chosen, rules for Wi-Fi coexistence found |
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
- **RAM**: answered in phase A - NimBLE leaves 105 KB internal RAM with everything running;
  Bluedroid (40 KB, fragmented) is not viable.
- **Android local-network rules**: recent Android versions restrict mDNS / local traffic in
  some cases; fall back to entering the IP shown on the clock.
- **Arduino-ESP32 3.x**: firmware is verified on 2.0.17 only; the API work should also
  add a core-3 CI build.
- Security: LAN-only HTTP with bearer tokens; HTTPS on the ESP32 is possible but heavy.
  Pairing code + rate limit + token revocation is the v1 trade-off.
