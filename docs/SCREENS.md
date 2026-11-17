# Screen layouts (320 x 240 landscape)

All screens are drawn into one 320x240 RGB565 canvas in PSRAM and pushed in one go
(~32 ms), so there is no flicker. The bottom strip (y 213-237) always shows three labels
centred over the physical buttons (x = 68, 160, 252).

## Home (day)

```
 y 0   [wx 19°C]          [* 2]           [sync][wifi][battery]
 y 28  [ 0 7 : 3 0 ]  (bubble digits 44x72)        [ AM ]
                                                   [ PM ]   <- active chip filled
 y110        Tuesday, 17 Nov 2026     (weekday coloured)
 y142  (character   [ bell 06:30 AM  tomorrow        ]
        on hill)    [ rotating: alarm name / jobs /  ]
                    [ stars / "Good morning, <name>!"]
 y213  [  <  ]          [ Alarms ]          [  >  ]
```

- Sky colours and character change with the time of day: morning (5-12) Sunny the sun,
  afternoon (12-17) Bun-Bun the bunny, evening (17-21) Miko the kitty, night Luna the moon.
- The info card's second line rotates every 5 s; battery-low and "time not checked
  online" warnings take priority.
- Rendering: the static layer (sky, digits, date, card, icons) is rebuilt every 5 s or on
  a button press; per frame only the character, the blinking colon and the button labels
  are drawn (~24 ms render + 32 ms push).

## Home (night mode)

Dark navy, very dim (brightness 1-5), small dim digits **with AM/PM**, date, next alarm,
sleeping moon with floating "z z Z". Redraws once a second. Any press = 30 s peek at the
normal screen; hold C = night mode off until the next night.

## Time not set

Shown until NTP / manual / RTC time is available: the sun asks "What time is it?" and
points grown-ups to *Settings > Set time*.

## Alarms

- **List:** Back + 10 rows (`06:30 AM`, name, repeat, on/off switch). B = edit,
  hold B = quick on/off.
- **Editor:** hour / minute / AM-PM chips, then Repeat, Days (Mon-first dots), Sound
  (plays a preview), Name (preset list; custom names via the phone page), Alarm on/off,
  Save. A/C change, B next, hold B save, hold A cancel.
- **Ringing:** flashing pastel background, excited character, alarm name, time with
  AM/PM, "Wake up, <name>!", big **Snooze N** (A) and **STOP** (B, C) buttons.

## Routine / Stars / Reminder

- **Routine:** period chip (Morning / Afternoon / Evening / Night jobs), stars today,
  progress bar, checklist rows with time; B ticks (+1 star, sound, green LEDs, star
  burst), B on a ticked row undoes it.
- **Stars:** today's stars in a big star, all-time total, badge, progress to the next badge.
- **Reminder pop-up:** "<name>, time to... Brush teeth", Later (10 min) / Done! / OK.

## Settings

Scrolling grouped list: Display, Sound, Lights, Alarms, Night mode, Time & Wi-Fi, Daily
routine, System. Rows show their current value; B on a value row enters edit mode
(yellow) where A/C change it live.

## Parent setup

QR code to join the clock's temporary access point, the 8-digit one-time password and
the page address (`http://192.168.4.1`, plus the home-network address when connected).

Screenshots of most screens are in `docs/screenshots/` (taken from the device).
