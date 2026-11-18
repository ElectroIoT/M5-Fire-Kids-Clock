#!/usr/bin/env python3
"""Cleanly restart the clock over USB (counts as a power-on reset, so any
reduced-volume mode after power dips is cleared).

Sends 'flashprep' first so the reset cannot catch the PSRAM mid-transfer
(Fire flash quirk), then pulses EN via the USB-serial RTS line.

    python tools/clean_reset.py COM16
"""
import sys
import time

import kc

port = sys.argv[1] if len(sys.argv) > 1 else "COM16"
s = kc.open_port(port)
kc.cmd(s, "flashprep", 0.8)
s.dtr = False
s.rts = True      # EN low
time.sleep(0.2)
s.rts = False     # release: boot
s.close()
print("reset sent")
