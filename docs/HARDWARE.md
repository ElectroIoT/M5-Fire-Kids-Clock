# Hardware & GPIO reference - M5Stack Fire (K007)

Verified on the connected unit: **ESP32-D0WDQ6 rev 1.0, 16 MB flash, PSRAM** (M5Unified
detects it as `board_M5Stack`, id 1). Sources: M5Stack Fire docs (PinMap, 2026-11-06),
M5Unified 0.3.0 pin tables.

| Function | GPIO | Driven by |
|---|---|---|
| LCD MOSI / MISO / SCK | 23 / 19 / 18 | M5GFX |
| LCD CS / DC / RST / backlight | 14 / 27 / 33 / 32 | M5GFX |
| microSD CS (shares LCD SPI) | 4 | Arduino `SD` |
| Button A / B / C | 39 / 38 / 37 | M5Unified `BtnA/B/C` |
| Speaker (internal DAC1, 1 W) | 25 | M5Unified `Speaker` (I2S DAC mode) |
| RGB LEDs, 10 x SK6812 (M5GO base) | 15 | Adafruit NeoPixel (RMT) |
| Microphone (analog, M5GO base) | 34 | not used |
| Internal I2C = Grove Port A | SDA 21 / SCL 22 | M5Unified `In_I2C` |
| Grove Port B | 26 (out) / 36 (in only) | free |
| Grove Port C | 16 / 17 | **unusable - wired to PSRAM** |

I2C devices on the internal bus: IP5306 power IC `0x75`, MPU6886 IMU `0x68` (or SH200Q
`0x6C` on older units), BMM150 magnetometer `0x10`.

All pin numbers live in `src/config/Pins.h`; the firmware never configures the display,
buttons or speaker by hand - M5Unified does it for the detected board.

## Notes

- **GPIO16/17** belong to the PSRAM. Never attach anything to them.
- **Display rotation:** with M5GFX 0.3.0 the upright landscape orientation on the Fire is
  rotation **0**; the firmware keeps M5Unified's default and only corrects it if the panel
  reports portrait (an early build forced rotation 1, which drew the UI sideways).
- **LED data line:** the Fire docs suggest `pinMode(15, OUTPUT_OPEN_DRAIN)`. The RMT
  driver re-applies push-pull on every frame (M5Unified's own LED driver does the same on
  purpose), and the LEDs were confirmed working this way on the test unit.
- **GPIO15 and Wi-Fi:** M5Unified drives GPIO15 low at boot because a floating GPIO15 can
  reduce Wi-Fi sensitivity with the M5GO base attached.
- **Speaker:** an 8-bit DAC through a small amplifier - expect a little hiss; melodies use
  a soft waveform to sound rounder than a square wave.
- **Battery:** 500 mAh. With the screen on it lasts only a few hours - this is a
  USB-powered bedside clock. The firmware turns on IP5306 "boost keep-on" so the power IC
  does not switch the clock off at light load.
- **IP5306 voltage:** the Fire's IP5306 reports only 0/25/50/75/100 % and no voltage
  (shows `0 mV`).
- **Flash + PSRAM share the SPI data lines.** If the chip is reset (EN / upload) while
  the PSRAM is mid-transfer, the flash may read as 0xFF until a full power-off. Observed
  repeatedly on the test unit (K007 "PSRAM 2.0", an older, now discontinued revision);
  avoided with `flashprep` before uploads.
- **Power button:** single click = power on / reset, double click = power off (not while
  on USB).
