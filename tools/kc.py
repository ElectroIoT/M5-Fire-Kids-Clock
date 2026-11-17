#!/usr/bin/env python3
"""Talk to the M5 Fire Kids Clock over USB serial without resetting it.

  python tools/kc.py COM16 cmd "status"          # run console command(s), print reply
  python tools/kc.py COM16 shot screen.png       # save a screenshot (PNG)
  python tools/kc.py COM16 log 10                # print serial log for N seconds

Requires: pip install pyserial pillow
"""
import sys
import time

import serial

W, H = 320, 240


def open_port(port):
    s = serial.Serial()
    s.port = port
    s.baudrate = 115200
    s.timeout = 0.2
    s.dtr = False  # keep EN / GPIO0 untouched -> no reset on open
    s.rts = False
    s.open()
    time.sleep(0.1)
    s.reset_input_buffer()
    return s


def cmd(s, line, wait=1.0):
    s.write((line + "\n").encode())
    end = time.time() + wait
    out = b""
    while time.time() < end:
        out += s.read(4096)
    return out.decode("utf-8", "replace")


def shot(s, path):
    from PIL import Image

    s.reset_input_buffer()
    s.write(b"shot\n")
    buf = b""
    end = time.time() + 30  # 153 KB at 115200 baud takes ~14 s
    while b"SHOT " not in buf and time.time() < end:
        buf += s.read(256)
    i = buf.find(b"SHOT ")
    j = buf.find(b"\n", i)
    while j < 0:
        buf += s.read(256)
        j = buf.find(b"\n", i)
    data = buf[j + 1:]
    need = W * H * 2
    while len(data) < need and time.time() < end:
        data += s.read(need - len(data))
    if len(data) < need:
        raise SystemExit("screenshot incomplete: %d/%d bytes" % (len(data), need))
    data = data[:need]
    img = Image.new("RGB", (W, H))
    px = img.load()
    for y in range(H):
        for x in range(W):
            o = (y * W + x) * 2
            v = (data[o] << 8) | data[o + 1]  # big-endian RGB565
            r = (v >> 11) & 0x1F
            g = (v >> 5) & 0x3F
            b = v & 0x1F
            px[x, y] = ((r * 527 + 23) >> 6, (g * 259 + 33) >> 6, (b * 527 + 23) >> 6)
    img = img.resize((W * 2, H * 2), Image.NEAREST)
    img.save(path)
    print("saved", path)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return
    port, action = sys.argv[1], sys.argv[2]
    s = open_port(port)
    if action == "cmd":
        for line in sys.argv[3:]:
            print(cmd(s, line, 1.5), end="")
    elif action == "shot":
        shot(s, sys.argv[3] if len(sys.argv) > 3 else "screen.png")
    elif action == "log":
        end = time.time() + float(sys.argv[3] if len(sys.argv) > 3 else 10)
        while time.time() < end:
            d = s.read(4096)
            if d:
                sys.stdout.write(d.decode("utf-8", "replace"))
                sys.stdout.flush()
    s.close()


if __name__ == "__main__":
    main()
