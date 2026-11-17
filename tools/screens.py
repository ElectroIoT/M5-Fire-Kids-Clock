#!/usr/bin/env python3
"""Capture a full set of README screenshots from a connected clock.

    python tools/screens.py COM16 docs/screenshots

Mutes the clock and turns LEDs off while capturing, temporarily enables alarm 1 and
clears the child's name so the pictures are generic. Afterwards it restores volume 6,
LEDs auto, alarm 1 off and resyncs the time over NTP - set the name again yourself
(`name ...`) if you use one.
"""
import os
import sys
import time

import kc


def main():
    port, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    s = kc.open_port(port)

    def c(*lines, wait=1.2):
        for line in lines:
            kc.cmd(s, line, wait)

    def shot(name):
        time.sleep(1.5)
        kc.shot(s, os.path.join(out, name + ".png"))

    c("vol 0", "led off", "name", "night off", "key a long", "key a long", "alarm 1 6:30 AM")
    c("settime 2026-11-18 7:22 AM")
    shot("home_morning")
    c("settime 2026-11-18 1:15 PM")
    shot("home_afternoon")
    c("settime 2026-11-18 6:20 PM")
    shot("home_evening")
    c("settime 2026-11-18 11:45 PM", wait=2)
    shot("home_night")
    c("night skip", "settime 2026-11-18 7:22 AM")
    c("ring 1", wait=2)
    shot("alarm_ringing")
    c("stop")
    c("key b")                          # dock: Alarms
    shot("alarm_list")
    c("key b")                          # edit alarm 1
    shot("alarm_edit")
    c("key a long", "key a long", "key c", "key b")  # cancel, home, Routine
    c("key b")                          # tick the first open job
    time.sleep(2.5)
    shot("routine")
    c("key a long", "key c", "key b")   # Stars
    shot("stars")
    c("key a long", "remind 3")
    shot("reminder")
    c("key c", "key c", "key b")        # dock: Settings
    shot("settings")
    c("key a long", "undo 1", "undo 2", "undo 3", "alarm 1 off", "vol 6", "led auto", "sync")
    s.close()
    print("done - remember to set the child's name again if you use one")


if __name__ == "__main__":
    main()
