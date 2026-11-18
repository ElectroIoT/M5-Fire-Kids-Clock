# Publishing to M5Burner

How to share this firmware in [M5Burner](https://docs.m5stack.com/en/uiflow/m5burner/publish)
(*USER CUSTOM > Publish*, logged in with an M5Stack community account).

> **Do not use M5Burner's "Export" on your own clock.** Export reads the whole flash from
> the device - including the saved Wi-Fi names and passwords, the child's name and the
> paired-phone tokens. Upload the clean `-full.bin` from the Releases page instead: it is
> built from source and its settings area (NVS, `0x9000`) is blank.

## Fields

| Field | Value |
|---|---|
| Name | `M5 Fire Kids Clock` |
| Version | `v1.1.0` |
| Device Type | `FIRE` (original M5Stack Fire K007 only) |
| Github | `https://github.com/ElectroIoT/M5-Fire-Kids-Clock` |
| FirmWare | `M5-Fire-Kids-Clock-v1.1.0-full.bin` (single image, flashed at `0x0`) |
| Cover | [`docs/images/m5burner_cover.png`](images/m5burner_cover.png) (800 x 600) |

## Description (paste as is)

```text
A friendly alarm clock for children on the original M5Stack Fire (K007).

- Big 12-hour clock with AM/PM and date, India time (IST), set from the internet (NTP)
- 10 alarms with cartoon tunes, snooze 5/10/15 min, school days / weekends / pick days
- Daily routine: 15 jobs from "Wake up" to "Sleep", a star for each, badges, reminders
- Cartoon friends for morning, afternoon, evening and night
- RGB side lights, dim night mode with a soft night light
- Weather (Open-Meteo, no API key), battery level, child's name greetings
- Parent setup from a phone browser; optional phone app over home Wi-Fi (no cloud)

First setup: on the clock open Settings > Parent setup (phone), scan the QR code
(or join the "KidsClock-XXXX" Wi-Fi with the password shown), open http://192.168.4.1
and enter your home Wi-Fi and the child's name. The time sets itself.
Buttons: A = left / previous, B = open / OK, C = right / next. Hold A = back.
Alarm ringing: A = snooze, B or C = stop.

Works with the plain Fire only - not Core2, CoreS3 or Basic.
Flashing this image erases the clock's saved settings; set it up again afterwards.
Guide, source and updates that keep settings: see the GitHub link.
```

## Making the image (maintainers)

```bash
pio run -e m5stack-fire
python ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32 merge_bin \
  -o release/M5-Fire-Kids-Clock-vX.Y.Z-full.bin --flash_mode keep --flash_freq keep --flash_size 16MB \
  0x1000 .pio/build/m5stack-fire/bootloader.bin \
  0x8000 .pio/build/m5stack-fire/partitions.bin \
  0xe000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
  0x10000 .pio/build/m5stack-fire/firmware.bin
```

Before uploading, check the image holds no personal data, for example
`grep -c -a "<your Wi-Fi name>" release/*.bin` must print `0`.
