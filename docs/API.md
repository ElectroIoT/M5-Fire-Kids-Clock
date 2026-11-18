# Local JSON API (for the companion app)

Base URL: `http://kidsclock.local:8080/api/v1` (or the IP shown on the clock).
Available whenever the clock is connected to the home Wi-Fi. Not reachable from the
internet; there is no cloud.

- Discovery: mDNS host `kidsclock.local`, DNS-SD service `_kidsclock._tcp` (port 8080,
  TXT `api=1`, `fw=<version>`).
- Format: JSON in and out, UTF-8. Errors: `{"ok": false, "error": "..."}` with an HTTP status.
- Times of day are always sent in both forms:
  `{"hour": 18, "minute": 30, "hour12": 6, "pm": true, "text": "06:30 PM"}`.
  When writing, send either `hour` (0-23) or `hour12` + `pm`, plus `minute`.
- The ESP32 web server closes the connection after every request and has only a few
  TCP slots: do not fire many requests in parallel, retry once on a connection error.
  Measured latency on the test unit: 16-144 ms (median ~70 ms).

## Pairing and authentication

1. On the clock: **Settings > Pair phone app** shows a 6-digit code (valid 2 minutes).
2. App: `POST /pair` with `{"code": "123456", "name": "Mum's phone"}` -> `{"ok": true, "token": "<32 hex>"}`.
3. Every other call (except `/info`): header `Authorization: Bearer <token>`.

Up to 4 phones. The code works once. 5 wrong codes close the window and lock pairing for
60 s. Tokens are stored on the clock and removed with *Pair phone app > Remove*.

| Status | Meaning |
|---|---|
| 401 | missing / unknown token |
| 403 | wrong pairing code |
| 409 | no pairing window open (or code already used) |
| 429 | locked after too many wrong codes |
| 507 | already 4 phones paired |

## Endpoints

| Method | Path | Body / query | Returns |
|---|---|---|---|
| GET | `/info` | - (no token) | `device`, `fw`, `api`, `id`, `paired_phones`, `pairing_open` |
| POST | `/pair` | `code`, `name` (no token) | `token` |
| GET | `/status` | - | time (valid, source, date, 12/24 h), Wi-Fi, battery, night mode, ringing alarm, next alarm, routine progress + stars, child name, uptime, `reset_reason`, `starts_since_power_on`, `volume_limited`, `last_event` |
| GET | `/alarms` | - | `alarms[10]` (number, enabled, time, repeat, repeat_name, days_mask, days_text, sound, sound_name, label, snoozed) and `sounds[]` names |
| PUT | `/alarms/{1..10}` | any of `enabled`, `time`, `repeat` (0 once, 1 daily, 2 school days, 3 weekends, 4 pick days), `days_mask` (bit0 = Sunday), `sound`, `label` | updated `alarm` |
| POST | `/alarms/{n}/test` | - | rings alarm n now |
| POST | `/alarm/stop` | - | `was_ringing` |
| POST | `/alarm/snooze` | - | `was_ringing` |
| GET | `/routine` | - | `jobs[18]` (number, name, enabled, used, period 0-3, period_name, time or null, remind, done), stars, `new_day_hour` |
| PUT | `/routine/{n}` | any of `name`, `enabled`, `remind`, `period`, `time` (`null` = no time) | updated `job` |
| POST | `/routine/{n}/done` | - | `stars_earned`, `all_done` (plays the celebration on the clock) |
| POST | `/routine/{n}/undo` | - | - |
| GET | `/settings` | - | child_name, volume, quiet_mode, button_beeps, alarm_fade_in, snooze_min, brightness, dim_idle_min, night{enabled,start,end,brightness,lullaby,wifi_off}, leds{enabled,brightness,night_glow}, weather{enabled,place,latitude,longitude}, routine{reminders,new_day_hour} |
| PUT | `/settings` | any subset of the above (same shape) | full settings |
| POST | `/time` | `year`, `month`, `day`, `hour12` + `pm` (or `hour`), `minute` | sets the clock by hand (no internet) |
| GET | `/events?after=N` | - | `last`, `events[]` with seq > N |

### Events

`seq`, `epoch` (0 if the clock had no time), `type`, optional `number` (alarm / job), `text`.

| type | When |
|---|---|
| `boot` | clock started (text includes the reset reason) |
| `power_weak` | restarted after a brownout - volume limited (is the M5GO base attached?) |
| `alarm_ring`, `alarm_snooze`, `alarm_stop` | alarm activity |
| `alarm_ignored` | an alarm stopped by itself after 3 auto-snoozes - nobody pressed a button |
| `routine_done`, `routine_period_done`, `routine_all_done` | jobs ticked |
| `battery_low` | battery at or below 25 % and not charging |

The log holds the last 32 events in RAM (cleared on restart; `boot` tells the app).

## Example

```bash
curl http://kidsclock.local:8080/api/v1/info
```

```bash
curl -X POST http://kidsclock.local:8080/api/v1/pair -d '{"code":"123456","name":"Test"}'
```

```bash
curl -H "Authorization: Bearer TOKEN" http://kidsclock.local:8080/api/v1/status
```

```bash
curl -X PUT -H "Authorization: Bearer TOKEN" http://kidsclock.local:8080/api/v1/alarms/1 -d '{"time":{"hour12":6,"minute":45,"pm":false},"enabled":true}'
```

`tools/api_test.py COM16` runs all of this automatically (21 checks) - it removes all
paired phones at the end, so only use it before real phones are paired.

## Security notes

LAN-only plain HTTP with bearer tokens (HTTPS on the ESP32 is possible but heavy). The
pairing code is shown only on the clock's screen, is single-use and rate-limited; tokens
are 128-bit random and compared in constant time. Anyone on the same Wi-Fi who has a
token can control the clock - remove phones you no longer use.
