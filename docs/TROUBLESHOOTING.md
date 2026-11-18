# Troubleshooting

| Problem | Fix |
|---|---|
| Picture is sideways / cut off | Old build forced rotation 1. Current firmware uses M5Unified's default (0 on M5GFX 0.3.0). If it is upside down on your unit, change the rotation logic in `DisplayManager::begin()`; console `rot 0` / `rot 2` tries it live. |
| Upload fails: "Failed to communicate with the flash chip" / "Packet content transfer stopped", or the clock stays dark after a reset with `flash read err, 1000` in the serial log | Fire PSRAM/flash quirk: a reset during a PSRAM transfer leaves the flash chip unreachable. **Fix now:** unplug USB, double-click the red button (power off), pull off the M5GO base for 10 s, reconnect. **Prevent it:** send `flashprep` before uploading (PlatformIO does it automatically via `tools/pio_flashprep.py`; for esptool type `flashprep` in a serial monitor first). Also use a good data cable and `upload_speed = 460800`. |
| Screen says "What time is it?" | Time unknown after a power loss. Connect Wi-Fi (*Settings > Parent setup*) or *Settings > Set time & date*. |
| Wi-Fi never connects | The Fire only supports **2.4 GHz**. Console `scan` lists networks; `status` shows the state. Check the password via the parent page. A backup network can be added. |
| Time is wrong by 5 h 30 min | Something set UTC as local time; use *Sync time now* or `settime`. The firmware uses `IST-5:30`. |
| Parent page does not open | The phone must be joined to `KidsClock-XXXX` (password on screen). Turn off mobile data if the phone keeps leaving the network. Page closes after 15 min idle - reopen it. |
| No sound | Volume 0? Quiet mode only mutes beeps, not alarms. Console `sound 2` plays a melody. |
| SD sound not used | FAT32 card, file in `/sounds/`, PCM WAV <= 3 MB, inserted before power-on. See [AUDIO.md](AUDIO.md). |
| LEDs dark | *Settings > LED lights* on? They only light for alarms, notifications, rewards, sunrise/morning or the night glow. Console `led reward` tests them. The LEDs are in the M5GO base - it must be attached. |
| Clock switches itself off on battery | Firmware enables IP5306 boost keep-on; if it still happens the battery is empty - this is a USB clock. |
| Alarm did not ring | Check it is ON, the days, and that the clock had a valid time. `alarms` on the console shows the next ring. One-time alarms switch off after ringing. |
| Routine ticks vanished | They reset once a day at *New day starts* (default 3:00 AM). |
| Factory reset | *Settings > Reset all settings* (erases alarms, routine, stars, Wi-Fi, name; restarts). |
| Restore the original firmware | Back it up before the first install (see [FLASHING.md](FLASHING.md)), then `esptool.py --port COMx write_flash 0 fire_backup_16MB.bin` |

Serial log at 115200 baud shows timestamped messages for Wi-Fi, NTP, alarms, routine,
weather and night mode; `status` prints a full snapshot.
