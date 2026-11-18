# PlatformIO pre-upload hook: tell a running clock to go quiet before esptool resets it.
#
# Why: on the M5Stack Fire the PSRAM shares data lines with the flash chip. If the
# upload's reset hits while the firmware is busy in PSRAM (it draws the screen there),
# the PSRAM can keep driving those lines and the flash reads as 0xFF -
# "Failed to communicate with the flash chip" / "flash read err, 1000" - until the
# board is fully powered off. "flashprep" stops all drawing first.
# Verified on a Fire K007 (PSRAM 2.0): 3/3 consecutive uploads succeeded with it,
# while uploads without it kept failing.
Import("env")  # noqa: F821  (provided by PlatformIO / SCons)


def flashprep(source, target, env):  # noqa: ARG001
    port = env.subst("$UPLOAD_PORT")
    if not port:
        return
    try:
        import time

        import serial

        s = serial.Serial()
        s.port = port
        s.baudrate = 115200
        s.timeout = 0.2
        s.dtr = False  # do not reset the board when opening the port
        s.rts = False
        s.open()
        s.reset_input_buffer()
        s.write(b"\nflashprep\n")
        time.sleep(0.6)
        reply = s.read(256).decode("utf-8", "replace")
        s.close()
        print("flashprep:", "clock is quiet" if "quiet" in reply else "no reply (clock not running this firmware?)")
    except Exception as exc:  # never block an upload because of this helper
        print("flashprep skipped:", exc)


env.AddPreAction("upload", flashprep)  # noqa: F821
