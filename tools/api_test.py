#!/usr/bin/env python3
"""End-to-end test of the clock's JSON API (phase B). Needs the clock on USB (to read
the pairing code from the console) and on the same Wi-Fi as this computer.

    python tools/api_test.py COM16 [clock-ip-or-kidsclock.local]

It pairs a test phone, exercises every endpoint, restores what it changed and removes
ALL paired phones at the end (so do not run it once real phones are paired).
"""
import json
import re
import statistics
import sys
import time
import urllib.error
import urllib.request

import kc

PORT = 8080
results = []
retries = 0


SER = None  # set in main(); used by the optional PROBE=1 restart detector
LAST_START = [None]


def probe(name):
    import os
    if not os.environ.get("PROBE") or SER is None:
        return
    SER.reset_input_buffer()
    SER.write(b"status\n")
    time.sleep(1.2)
    m = re.search(r"start #(\d+)", SER.read(8192).decode("utf-8", "replace"))
    n = int(m.group(1)) if m else -1
    if LAST_START[0] is not None and n != LAST_START[0]:
        print("     !!! clock restarted during/after: %s" % name)
    LAST_START[0] = n


def check(name, ok, detail=""):
    results.append((name, ok))
    probe(name)
    print("%-4s %-42s %s" % ("PASS" if ok else "FAIL", name, detail))


def call(host, method, path, body=None, token=None, timeout=15):
    for attempt in range(2):  # one retry: Wi-Fi links drop the odd packet
        try:
            return _call(host, method, path, body, token, timeout)
        except (urllib.error.URLError, ConnectionError) as e:
            global retries
            retries += 1
            print("     (retry after %s)" % type(e).__name__)
            if attempt:
                raise
            time.sleep(1)


def _call(host, method, path, body=None, token=None, timeout=15):
    time.sleep(0.15)  # pace like a real app; the ESP32 has few TCP slots
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request("http://%s:%d%s" % (host, PORT, path), data=data, method=method)
    req.add_header("Content-Type", "application/json")
    if token:
        req.add_header("Authorization", "Bearer " + token)
    t0 = time.time()
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, json.loads(r.read() or b"{}"), time.time() - t0
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read() or b"{}"), time.time() - t0


def main():
    port = sys.argv[1]
    global SER
    s = kc.open_port(port)
    SER = s
    host = sys.argv[2] if len(sys.argv) > 2 else re.search(r"connected\s+([\d.]+)", kc.cmd(s, "status", 1.5)).group(1)
    print("clock:", host)

    st, j, _ = call(host, "GET", "/api/v1/info")
    check("info without token", st == 200 and j.get("api") == 1, j.get("fw", ""))
    st, j, _ = call(host, "GET", "/api/v1/status")
    check("status without token -> 401", st == 401)
    st, j, _ = call(host, "GET", "/api/v1/status", token="0" * 32)
    check("status with wrong token -> 401", st == 401)
    st, j, _ = call(host, "POST", "/api/v1/pair", {"code": "000000"})
    check("pair with no window open -> 409", st == 409)

    out = kc.cmd(s, "pair", 2.0)
    code = re.search(r"pairing code (\d{6})", out).group(1)
    wrong = "%06d" % ((int(code) + 1) % 1000000)
    st, j, _ = call(host, "POST", "/api/v1/pair", {"code": wrong, "name": "Test PC"})
    check("pair with wrong code -> 403", st == 403)
    st, j, _ = call(host, "POST", "/api/v1/pair", {"code": code, "name": "Test PC"})
    token = j.get("token", "")
    check("pair with right code -> token", st == 200 and len(token) == 32)
    st, j, _ = call(host, "POST", "/api/v1/pair", {"code": code, "name": "Again"})
    check("code works only once -> 409", st == 409)
    kc.cmd(s, "key a", 1.0)  # leave the pairing screen

    st, j, _ = call(host, "GET", "/api/v1/status", token=token)
    check("status", st == 200 and "time" in j and "routine" in j,
          "%s, %s" % (j.get("time", {}).get("text"), j.get("time", {}).get("date")))

    st, j, _ = call(host, "GET", "/api/v1/alarms", token=token)
    alarms = j.get("alarms", [])
    check("alarms list", st == 200 and len(alarms) == 10, "%d alarms, %d sounds" % (len(alarms), len(j.get("sounds", []))))
    a10 = alarms[9]
    st, j, _ = call(host, "PUT", "/api/v1/alarms/10",
                    {"label": "API test", "time": {"hour12": 7, "minute": 45, "pm": True}, "enabled": False}, token)
    na = j.get("alarm", {})
    check("alarm update (12-hour input)", st == 200 and na.get("label") == "API test" and na.get("hour") == 19,
          na.get("text", ""))
    st, j, _ = call(host, "PUT", "/api/v1/alarms/10", {"time": {"hour": 25}}, token)
    check("alarm update with bad time -> 400", st == 400)
    st, j, _ = call(host, "PUT", "/api/v1/alarms/10",
                    {"label": a10["label"], "time": {"hour": a10["hour"], "minute": a10["minute"]},
                     "enabled": a10["enabled"], "sound": a10["sound"], "repeat": a10["repeat"], "days_mask": a10["days_mask"]},
                    token)
    check("alarm restored", st == 200 and j["alarm"]["label"] == a10["label"])

    st, j, _ = call(host, "GET", "/api/v1/routine", token=token)
    jobs = [x for x in j.get("jobs", []) if x["used"]]
    check("routine list", st == 200 and len(jobs) > 0, "%d jobs" % len(jobs))
    job = next((x for x in jobs if not x["done"]), None)
    if job:
        before = call(host, "GET", "/api/v1/events?after=0", token=token)[1].get("last", 0)
        st, j, _ = call(host, "POST", "/api/v1/routine/%d/done" % job["number"], token=token)
        check("tick a job", st == 200 and j.get("stars_earned", 0) >= 1, job["name"])
        st, j, _ = call(host, "GET", "/api/v1/events?after=%d" % before, token=token)
        types = [e["type"] for e in j.get("events", [])]
        check("event logged for the tick", "routine_done" in types, ",".join(types))
        st, j, _ = call(host, "POST", "/api/v1/routine/%d/undo" % job["number"], token=token)
        check("undo the tick", st == 200)

    st, j, _ = call(host, "GET", "/api/v1/settings", token=token)
    sset = j.get("settings", {})
    check("settings", st == 200 and "night" in sset, "volume %s, night %s-%s" % (
        sset.get("volume"), sset.get("night", {}).get("start", {}).get("text"), sset.get("night", {}).get("end", {}).get("text")))
    st, j, _ = call(host, "PUT", "/api/v1/settings", {"volume": sset.get("volume")}, token)
    check("settings write (same value)", st == 200)

    st, j, _ = call(host, "GET", "/api/v1/nope", token=token)
    check("unknown endpoint -> 404", st == 404)

    # Latency with Wi-Fi modem sleep on (normal operation)
    times = []
    for _ in range(20):
        st, j, dt = call(host, "GET", "/api/v1/status", token=token)
        if st == 200:
            times.append(dt * 1000)
        time.sleep(0.3)
    if times:
        check("20 x status latency", len(times) == 20, "min %.0f / median %.0f / max %.0f ms" % (
            min(times), statistics.median(times), max(times)))

    out = kc.cmd(s, "unpair", 1.5)
    st, j, _ = call(host, "GET", "/api/v1/status", token=token)
    check("token rejected after unpair", st == 401)
    s.close()
    print("\n%d passed, %d failed" % (sum(1 for _, ok in results if ok), sum(1 for _, ok in results if not ok)))


if __name__ == "__main__":
    main()
