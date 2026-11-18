# Alarm scheduling

## Model

Up to **10 alarms**, stored as one versioned, CRC-checked NVS record (`alarms`).

| Field | Notes |
|---|---|
| enabled | on / off |
| hour, minute | stored 24-hour (0-23); always shown and edited as 12-hour + AM/PM |
| repeat | Once, Every day, School days (Mon-Fri), Weekends (Sat+Sun), Pick days |
| days | bit mask, bit0 = Sunday ... bit6 = Saturday (used for "Pick days") |
| sound | Gentle wake, Cartoon alarm, Morning song, Twinkle star, Birdies, SD alarm1/2/morning |
| label | up to 23 characters ("School Wake Up") |

Three presets come **switched off** so a new clock never rings by surprise:
School Wake Up 6:30 AM (Mon-Fri), Homework Time 5:00 PM, Bedtime 9:00 PM.

## Engine (`AlarmManager`)

- Runs every loop pass from the ESP32 system clock - **no Wi-Fi needed**.
- Works on whole minutes since 1970 (`epoch / 60`) and converts with the fixed IST offset,
  so midnight, noon and weekday changes are exact (unit-tested on the host).
- Each alarm remembers the minute it last fired, so it **can never ring twice in the same
  minute**, even if the clock is set backwards.
- **Catch-up:** if the clock jumps forward by up to 3 minutes (NTP correction, a slow
  frame), alarms inside the skipped minutes still ring. Bigger jumps (first NTP sync
  after power-on, manual set) re-anchor without replaying old alarms.
- **Reboot-safe:** the last processed minute is kept in RTC memory across watchdog /
  software resets, so a crash neither loses nor repeats an alarm. After a power loss the
  time is unknown until NTP / manual / RTC; alarms wait for a valid time.
- **One-time** alarms switch themselves off after ringing (saved).
- Two alarms at the same minute: the second rings right after the first is stopped.
- No clock = no alarms: with an unset clock the engine does nothing rather than guess.

## Ringing

- Dedicated screen, excited character, flashing pink/red LEDs, screen brightness at
  least 8/10 (also during night mode).
- Sound loops; with *Alarm fade-in* the volume rises from 25 % to 100 % over 30 s.
  Alarms ignore quiet mode and never play below volume 4.
- **A = Snooze** (5 / 10 / 15 min, *Settings > Snooze time*), **B or C = Stop**.
- No response for 5 minutes -> automatic snooze; after 3 automatic snoozes it stops.
- **Restart while ringing** (power dip, crash, watchdog): the alarm rings again right after
  boot if it is still within its 5-minute ringing window (kept in RTC memory). Verified on
  the device: back ringing ~1.6 s after a brownout restart.
- Stopping or snoozing stops the sound instantly (`M5.Speaker.stop()`).

## Editing

- On the clock: *Alarms* from the home dock or *Settings > Edit alarms*.
- Phone: Parent setup > *Alarms* (also custom names).
- Serial: `alarm 1 6:30 AM`, `alarm 1 off`, `ring 1` (test), `stop`, `snooze`, `alarms`.

## Tested on the device

Real-time ring at the exact minute (07:27 PM), snooze 5 min and re-ring, 12:00 AM daily
alarm across midnight (date rolled to the next weekday), one-time alarms disabled after
ringing, alarms kept after reboot. See [TESTING.md](TESTING.md).
