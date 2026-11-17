# Audio & microSD sounds

## How it plays

- M5Unified `Speaker` drives the Fire's internal DAC (GPIO25) through I2S in the
  background. The firmware never blocks: melodies are fed note by note into the
  speaker's 2-slot queue from the main loop (`AudioManager::feed()`).
- Channel 0: alarms, previews, lullaby (can loop). Channel 1: short effects (click,
  task done, reward, notify), so a click never cuts an alarm off.
- A soft 16-sample waveform replaces the default square wave for a rounder sound.
- `stop()` silences everything immediately (used by Snooze / Stop).

## Built-in sounds

| Alarm sounds | Effects |
|---|---|
| Gentle wake (rising arpeggio) | Button click |
| Cartoon alarm (boingy, fast) | Task done (C-E-G-C) |
| Morning song - *Frère Jacques* (traditional) | Reward fanfare |
| Twinkle star - *Twinkle Twinkle Little Star* (traditional) | Notification |
| Birdies (chirps) | Morning chime, Bedtime - *Brahms' Lullaby* (public domain) |

## Volume

- *Settings > Volume* 0-10 (master volume). Alarms never play below 4.
- *Quiet mode* silences clicks and effects; **alarms still ring** (a muted alarm clock
  would be unsafe).
- *Alarm fade-in*: 25 % -> 100 % over 30 s.

## microSD WAV files (optional)

Format the card **FAT32** (max 16 GB on the Fire), create a `sounds` folder:

| File | Used for |
|---|---|
| `/sounds/alarm1.wav` | alarm sound "SD: alarm1" |
| `/sounds/alarm2.wav` | alarm sound "SD: alarm2" |
| `/sounds/morning.wav` | alarm sound "SD: morning" and the morning chime |
| `/sounds/bedtime.wav` | bedtime lullaby (replaces the built-in one) |

Requirements: **PCM WAV**, 8- or 16-bit, mono (stereo works), 8-44.1 kHz, max **3 MB**
(the file is loaded into PSRAM and played with `M5.Speaker.playWav()`). Suggested
conversion:

```bash
ffmpeg -i input.mp3 -ac 1 -ar 16000 -sample_fmt s16 alarm1.wav
```

Insert the card before powering on (detected at boot; *Settings > microSD sounds* shows
the state). Missing file -> the alarm falls back to the Cartoon alarm melody.

**MP3 is not decoded** on purpose: the Fire's 1 W DAC speaker gains nothing from MP3 and
a decoder library would add flash, RAM and CPU load. Convert MP3s to WAV as above.

*microSD playback is implemented but was not tested on hardware (no card in the test unit).*
