#include "ApiServer.h"
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>
#include <math.h>
#include <uri/UriBraces.h>
#include "EventLog.h"
#include "Pairing.h"
#include "../config/Config.h"
#include "../core/AlarmLogic.h"
#include "../core/TimeLogic.h"
#include "../alarm/AlarmManager.h"
#include "../audio/AudioManager.h"
#include "../audio/SoundList.h"
#include "../clock/ClockManager.h"
#include "../power/BatteryManager.h"
#include "../routine/RoutineManager.h"
#include "../settings/SettingsManager.h"
#include "../ui/UiHooks.h"
#include "../ui/UiManager.h"
#include "../app/NightMode.h"
#include "../wifi/WiFiLink.h"

namespace api {

static WebServer server(kPort);
static bool started = false;
static bool mdnsUp = false;
static uint32_t reqCount = 0;

uint32_t requests() { return reqCount; }

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void send(int code, JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", out);
}

static void error(int code, const char* msg) {
  JsonDocument d;
  d["ok"] = false;
  d["error"] = msg;
  send(code, d);
}

static bool authed() {
  ++reqCount;
  const String h = server.header("Authorization");
  if (h.startsWith("Bearer ") && Pair.check(h.c_str() + 7)) return true;
  error(401, "not paired - pair the app on the clock first");
  return false;
}

static bool parseBody(JsonDocument& doc) {
  const String body = server.arg("plain");
  if (body.length() > 2048 || deserializeJson(doc, body) != DeserializationError::Ok || !doc.is<JsonObject>()) {
    error(400, "expected a JSON object");
    return false;
  }
  return true;
}

// 12-hour + 24-hour representation of a time of day.
static void timeOut(JsonObject o, int hour24, int minute) {
  int h12;
  bool pm;
  tl::to12h(hour24, h12, pm);
  char t[12];
  tl::formatHM12AmPm(t, sizeof t, hour24, minute);
  o["hour"] = hour24;
  o["minute"] = minute;
  o["hour12"] = h12;
  o["pm"] = pm;
  o["text"] = t;
}

// Accepts {"hour":18,"minute":5} or {"hour12":6,"pm":true,"minute":5}. Returns minute of day or -1.
static int timeIn(JsonVariantConst v, int defHour, int defMinute) {
  if (!v.is<JsonObjectConst>()) return -1;
  int hour = defHour, minute = v["minute"] | defMinute;
  if (v["hour12"].is<int>()) hour = tl::to24h(v["hour12"].as<int>(), v["pm"] | false);
  else if (v["hour"].is<int>()) hour = v["hour"].as<int>();
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return -1;
  return hour * 60 + minute;
}

static int pathIndex(int max) {
  const int n = server.pathArg(0).toInt();
  return (n >= 1 && n <= max) ? n - 1 : -1;
}

// ---------------------------------------------------------------------------
// Open endpoints
// ---------------------------------------------------------------------------
static void hInfo() {
  ++reqCount;
  JsonDocument d;
  d["ok"] = true;
  d["device"] = "M5 Fire Kids Clock";
  d["fw"] = KC_FW_VERSION;
  d["api"] = 1;
  d["id"] = WiFi.macAddress();
  d["paired_phones"] = Pair.count();
  d["pairing_open"] = Pair.windowOpen();
  send(200, d);
}

static void hPair() {
  ++reqCount;
  JsonDocument in;
  if (!parseBody(in)) return;
  char token[33];
  const Pairing::Result r = Pair.pair(in["code"] | "", in["name"] | "Phone", token);
  switch (r) {
    case Pairing::Result::Ok: {
      JsonDocument d;
      d["ok"] = true;
      d["token"] = token;
      send(200, d);
      UI.toast("Phone paired!");
      return;
    }
    case Pairing::Result::NoWindow: error(409, "open Settings > Pair phone app on the clock first"); return;
    case Pairing::Result::WrongCode: error(403, "wrong code"); return;
    case Pairing::Result::LockedOut: error(429, "too many wrong codes - wait a minute"); return;
    default: error(507, "4 phones already paired - remove one on the clock"); return;
  }
}

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------
static void hStatus() {
  if (!authed()) return;
  JsonDocument d;
  d["ok"] = true;
  const LocalTime t = Clock.now();
  JsonObject time = d["time"].to<JsonObject>();
  time["valid"] = t.valid;
  time["epoch"] = (uint32_t)Clock.nowEpoch();
  time["source"] = Clock.sourceName();
  if (t.valid) {
    timeOut(time, t.hour, t.minute);
    char date[40];
    tl::formatDateLong(date, sizeof date, t.wday, t.year, t.month, t.day);
    time["date"] = date;
  }
  JsonObject w = d["wifi"].to<JsonObject>();
  w["rssi"] = Net.rssi();
  w["ip"] = Net.ip().toString();
  JsonObject b = d["battery"].to<JsonObject>();
  b["level"] = Battery.level();
  b["charging"] = Battery.charging();
  d["night_mode"] = NightMode.active();
  d["ringing"] = Alarms.ringing() ? Alarms.ringingIndex() + 1 : 0;
  int idx, mins, h, m;
  bool snooze;
  if (Alarms.next(idx, mins, h, m, snooze)) {
    JsonObject n = d["next_alarm"].to<JsonObject>();
    n["number"] = idx + 1;
    n["label"] = Alarms.alarm(idx).label;
    n["in_minutes"] = mins;
    n["snooze"] = snooze;
    timeOut(n, h, m);
  }
  JsonObject r = d["routine"].to<JsonObject>();
  int done, total;
  Routine.progressAll(done, total);
  r["done"] = done;
  r["total"] = total;
  r["stars_today"] = Routine.starsToday();
  r["stars_total"] = Routine.starsTotal();
  d["child_name"] = AppCfg.get().childName;
  d["fw"] = KC_FW_VERSION;
  d["uptime_s"] = millis() / 1000;
  d["reset_reason"] = resetReasonText();
  d["starts_since_power_on"] = bootCount();
  d["volume_limited"] = esp_reset_reason() == ESP_RST_BROWNOUT;
  d["last_event"] = Events.lastSeq();
  send(200, d);
}

// ---------------------------------------------------------------------------
// Alarms
// ---------------------------------------------------------------------------
static void alarmOut(JsonObject o, int i) {
  const al::AlarmConfig& a = Alarms.alarm(i);
  o["number"] = i + 1;
  o["enabled"] = a.enabled != 0;
  timeOut(o, a.hour, a.minute);
  o["repeat"] = a.repeat;
  o["repeat_name"] = al::repeatName(a.repeat);
  o["days_mask"] = a.days;  // bit0 = Sunday
  char days[32];
  al::describeDays(a, days, sizeof days);
  o["days_text"] = days;
  o["sound"] = a.sound;
  o["sound_name"] = sounds::alarmSoundName(a.sound);
  o["label"] = a.label;
  o["snoozed"] = Alarms.snoozed(i);
}

static void hAlarms() {
  if (!authed()) return;
  JsonDocument d;
  d["ok"] = true;
  JsonArray arr = d["alarms"].to<JsonArray>();
  for (int i = 0; i < KC_MAX_ALARMS; ++i) alarmOut(arr.add<JsonObject>(), i);
  JsonArray snd = d["sounds"].to<JsonArray>();
  for (int s = 0; s < sounds::ALARM_SOUND_COUNT; ++s) snd.add(sounds::alarmSoundName(s));
  send(200, d);
}

static void hAlarmPut() {
  if (!authed()) return;
  const int i = pathIndex(KC_MAX_ALARMS);
  if (i < 0) return error(404, "alarm number 1..10");
  JsonDocument in;
  if (!parseBody(in)) return;
  al::AlarmConfig a = Alarms.alarm(i);
  if (in["enabled"].is<bool>()) a.enabled = in["enabled"].as<bool>() ? 1 : 0;
  if (!in["time"].isNull()) {
    const int mod = timeIn(in["time"], a.hour, a.minute);
    if (mod < 0) return error(400, "bad time");
    a.hour = (uint8_t)(mod / 60);
    a.minute = (uint8_t)(mod % 60);
  }
  if (in["repeat"].is<int>()) a.repeat = (uint8_t)in["repeat"].as<int>();
  if (in["days_mask"].is<int>()) a.days = (uint8_t)in["days_mask"].as<int>();
  if (in["sound"].is<int>()) a.sound = (uint8_t)in["sound"].as<int>();
  if (in["label"].is<const char*>()) strlcpy(a.label, in["label"].as<const char*>(), sizeof a.label);
  if (a.repeat >= al::REPEAT_COUNT || a.sound >= sounds::ALARM_SOUND_COUNT) return error(400, "bad repeat or sound");
  al::sanitize(a);
  Alarms.alarm(i) = a;
  Alarms.save();
  UI.invalidate();
  JsonDocument d;
  d["ok"] = true;
  alarmOut(d["alarm"].to<JsonObject>(), i);
  send(200, d);
}

static void hAlarmTest() {
  if (!authed()) return;
  const int i = pathIndex(KC_MAX_ALARMS);
  if (i < 0) return error(404, "alarm number 1..10");
  Alarms.test(i);
  JsonDocument d;
  d["ok"] = true;
  send(200, d);
}

static void hAlarmStop() {
  if (!authed()) return;
  const bool was = Alarms.ringing();
  Alarms.stop();
  JsonDocument d;
  d["ok"] = true;
  d["was_ringing"] = was;
  send(200, d);
}

static void hAlarmSnooze() {
  if (!authed()) return;
  const bool was = Alarms.ringing();
  Alarms.snooze();
  JsonDocument d;
  d["ok"] = true;
  d["was_ringing"] = was;
  send(200, d);
}

// ---------------------------------------------------------------------------
// Routine
// ---------------------------------------------------------------------------
static void taskOut(JsonObject o, int i) {
  const RoutineTask& t = Routine.task(i);
  o["number"] = i + 1;
  o["name"] = t.name;
  o["enabled"] = t.enabled != 0;
  o["used"] = Routine.used(i);
  o["period"] = t.period;
  o["period_name"] = tl::periodName((tl::DayPeriod)t.period);
  o["remind"] = t.remind != 0;
  o["done"] = Routine.done(i);
  if (t.minute >= 0) timeOut(o["time"].to<JsonObject>(), t.minute / 60, t.minute % 60);
  else o["time"] = nullptr;
}

static void hRoutine() {
  if (!authed()) return;
  JsonDocument d;
  d["ok"] = true;
  d["stars_today"] = Routine.starsToday();
  d["stars_total"] = Routine.starsTotal();
  d["new_day_hour"] = AppCfg.get().routineResetHour;
  JsonArray arr = d["jobs"].to<JsonArray>();
  for (int i = 0; i < KC_MAX_TASKS; ++i) taskOut(arr.add<JsonObject>(), i);
  send(200, d);
}

static void hRoutinePut() {
  if (!authed()) return;
  const int i = pathIndex(KC_MAX_TASKS);
  if (i < 0) return error(404, "job number out of range");
  JsonDocument in;
  if (!parseBody(in)) return;
  RoutineTask t = Routine.task(i);
  if (in["name"].is<const char*>()) strlcpy(t.name, in["name"].as<const char*>(), sizeof t.name);
  if (in["enabled"].is<bool>()) t.enabled = in["enabled"].as<bool>() ? 1 : 0;
  if (in["remind"].is<bool>()) t.remind = in["remind"].as<bool>() ? 1 : 0;
  if (in["period"].is<int>()) {
    const int p = in["period"].as<int>();
    if (p < 0 || p >= tl::PERIOD_COUNT) return error(400, "period 0..3");
    t.period = (uint8_t)p;
  }
  if (in["time"].isNull() && in.containsKey("time")) {
    t.minute = -1;
  } else if (!in["time"].isNull()) {
    const int mod = timeIn(in["time"], t.minute >= 0 ? t.minute / 60 : 7, t.minute >= 0 ? t.minute % 60 : 0);
    if (mod < 0) return error(400, "bad time");
    t.minute = (int16_t)mod;
  }
  if (!t.name[0]) t.enabled = 0;
  Routine.task(i) = t;
  Routine.saveConfig();
  UI.invalidate();
  JsonDocument d;
  d["ok"] = true;
  taskOut(d["job"].to<JsonObject>(), i);
  send(200, d);
}

static void hRoutineDone() {
  if (!authed()) return;
  const int i = pathIndex(KC_MAX_TASKS);
  if (i < 0 || !Routine.used(i)) return error(404, "no such job");
  bool all = false;
  const int earned = Routine.complete(i, &all);
  if (earned) uihooks::celebrate(all);
  UI.invalidate();
  JsonDocument d;
  d["ok"] = true;
  d["stars_earned"] = earned;
  d["all_done"] = all;
  send(200, d);
}

static void hRoutineUndo() {
  if (!authed()) return;
  const int i = pathIndex(KC_MAX_TASKS);
  if (i < 0 || !Routine.used(i)) return error(404, "no such job");
  Routine.undo(i);
  UI.invalidate();
  JsonDocument d;
  d["ok"] = true;
  send(200, d);
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
static void settingsOut(JsonObject o) {
  const Settings& s = AppCfg.get();
  o["child_name"] = s.childName;
  o["volume"] = s.volume;
  o["quiet_mode"] = s.muted != 0;
  o["button_beeps"] = s.clickSounds != 0;
  o["alarm_fade_in"] = s.gradualVolume != 0;
  o["snooze_min"] = s.snoozeMin;
  o["brightness"] = s.brightness;
  o["dim_idle_min"] = s.idleDimMin;
  JsonObject n = o["night"].to<JsonObject>();
  n["enabled"] = s.nightEnabled != 0;
  timeOut(n["start"].to<JsonObject>(), s.nightStartMin / 60, s.nightStartMin % 60);
  timeOut(n["end"].to<JsonObject>(), s.nightEndMin / 60, s.nightEndMin % 60);
  n["brightness"] = s.nightBrightness;
  n["lullaby"] = s.bedtimeMusic != 0;
  n["wifi_off"] = s.nightWifiOff != 0;
  JsonObject l = o["leds"].to<JsonObject>();
  l["enabled"] = s.ledEnabled != 0;
  l["brightness"] = s.ledBrightness;
  l["night_glow"] = s.ledNightGlow != 0;
  JsonObject w = o["weather"].to<JsonObject>();
  w["enabled"] = s.weatherEnabled != 0;
  w["place"] = s.city;
  if (isnan(s.latitude)) w["latitude"] = nullptr; else w["latitude"] = s.latitude;
  if (isnan(s.longitude)) w["longitude"] = nullptr; else w["longitude"] = s.longitude;
  JsonObject r = o["routine"].to<JsonObject>();
  r["reminders"] = s.routineReminders != 0;
  r["new_day_hour"] = s.routineResetHour;
}

template <typename T>
static void setU8(JsonVariantConst v, uint8_t& dst, int lo, int hi) {
  if (v.is<T>()) {
    const int x = (int)v.as<T>();
    if (x >= lo && x <= hi) dst = (uint8_t)x;
  }
}

static void hSettings() {
  if (!authed()) return;
  JsonDocument d;
  d["ok"] = true;
  settingsOut(d["settings"].to<JsonObject>());
  send(200, d);
}

static void hSettingsPut() {
  if (!authed()) return;
  JsonDocument in;
  if (!parseBody(in)) return;
  Settings& s = AppCfg.get();
  if (in["child_name"].is<const char*>()) strlcpy(s.childName, in["child_name"].as<const char*>(), sizeof s.childName);
  setU8<int>(in["volume"], s.volume, 0, 10);
  setU8<bool>(in["quiet_mode"], s.muted, 0, 1);
  setU8<bool>(in["button_beeps"], s.clickSounds, 0, 1);
  setU8<bool>(in["alarm_fade_in"], s.gradualVolume, 0, 1);
  if (in["snooze_min"].is<int>()) {
    const int v = in["snooze_min"].as<int>();
    if (v == 5 || v == 10 || v == 15) s.snoozeMin = (uint8_t)v;
  }
  setU8<int>(in["brightness"], s.brightness, 1, 10);
  setU8<int>(in["dim_idle_min"], s.idleDimMin, 0, 30);
  JsonVariantConst n = in["night"];
  if (n.is<JsonObjectConst>()) {
    setU8<bool>(n["enabled"], s.nightEnabled, 0, 1);
    int v = timeIn(n["start"], s.nightStartMin / 60, s.nightStartMin % 60);
    if (v >= 0) s.nightStartMin = (uint16_t)v;
    v = timeIn(n["end"], s.nightEndMin / 60, s.nightEndMin % 60);
    if (v >= 0) s.nightEndMin = (uint16_t)v;
    setU8<int>(n["brightness"], s.nightBrightness, 1, 5);
    setU8<bool>(n["lullaby"], s.bedtimeMusic, 0, 1);
    setU8<bool>(n["wifi_off"], s.nightWifiOff, 0, 1);
  }
  JsonVariantConst l = in["leds"];
  if (l.is<JsonObjectConst>()) {
    setU8<bool>(l["enabled"], s.ledEnabled, 0, 1);
    setU8<int>(l["brightness"], s.ledBrightness, 1, 10);
    setU8<bool>(l["night_glow"], s.ledNightGlow, 0, 1);
  }
  JsonVariantConst w = in["weather"];
  if (w.is<JsonObjectConst>()) {
    setU8<bool>(w["enabled"], s.weatherEnabled, 0, 1);
    if (w["place"].is<const char*>()) strlcpy(s.city, w["place"].as<const char*>(), sizeof s.city);
    if (w["latitude"].is<float>()) s.latitude = w["latitude"].as<float>();
    if (w["longitude"].is<float>()) s.longitude = w["longitude"].as<float>();
  }
  JsonVariantConst r = in["routine"];
  if (r.is<JsonObjectConst>()) {
    setU8<bool>(r["reminders"], s.routineReminders, 0, 1);
    setU8<int>(r["new_day_hour"], s.routineResetHour, 0, 23);
  }
  AppCfg.save();
  Audio.applyVolume();
  UI.invalidate();
  JsonDocument d;
  d["ok"] = true;
  settingsOut(d["settings"].to<JsonObject>());
  send(200, d);
}

// ---------------------------------------------------------------------------
// Time + events
// ---------------------------------------------------------------------------
static void hTime() {
  if (!authed()) return;
  JsonDocument in;
  if (!parseBody(in)) return;
  const int mod = timeIn(in.as<JsonVariantConst>(), 7, 0);
  if (mod < 0 || !Clock.setManual(in["year"] | 0, in["month"] | 0, in["day"] | 0, mod / 60, mod % 60))
    return error(400, "need year, month, day and hour12+pm (or hour) and minute");
  JsonDocument d;
  d["ok"] = true;
  send(200, d);
}

static void hEvents() {
  if (!authed()) return;
  const uint32_t after = (uint32_t)strtoul(server.arg("after").c_str(), nullptr, 10);
  static Event buf[EventLog::kSize];
  const int n = Events.since(after, buf, EventLog::kSize);
  JsonDocument d;
  d["ok"] = true;
  d["last"] = Events.lastSeq();
  JsonArray arr = d["events"].to<JsonArray>();
  for (int i = 0; i < n; ++i) {
    JsonObject e = arr.add<JsonObject>();
    e["seq"] = buf[i].seq;
    e["epoch"] = buf[i].epoch;
    e["type"] = EventLog::typeName(buf[i].type);
    if (buf[i].index >= 0) e["number"] = buf[i].index + 1;
    e["text"] = buf[i].text;
  }
  send(200, d);
}

// ---------------------------------------------------------------------------
void begin() {
  static const char* headers[] = {"Authorization"};
  server.collectHeaders(headers, 1);
  server.on("/api/v1/info", HTTP_GET, hInfo);
  server.on("/api/v1/pair", HTTP_POST, hPair);
  server.on("/api/v1/status", HTTP_GET, hStatus);
  server.on("/api/v1/alarms", HTTP_GET, hAlarms);
  server.on(UriBraces("/api/v1/alarms/{}"), HTTP_PUT, hAlarmPut);
  server.on(UriBraces("/api/v1/alarms/{}/test"), HTTP_POST, hAlarmTest);
  server.on("/api/v1/alarm/stop", HTTP_POST, hAlarmStop);
  server.on("/api/v1/alarm/snooze", HTTP_POST, hAlarmSnooze);
  server.on("/api/v1/routine", HTTP_GET, hRoutine);
  server.on(UriBraces("/api/v1/routine/{}"), HTTP_PUT, hRoutinePut);
  server.on(UriBraces("/api/v1/routine/{}/done"), HTTP_POST, hRoutineDone);
  server.on(UriBraces("/api/v1/routine/{}/undo"), HTTP_POST, hRoutineUndo);
  server.on("/api/v1/settings", HTTP_GET, hSettings);
  server.on("/api/v1/settings", HTTP_PUT, hSettingsPut);
  server.on("/api/v1/time", HTTP_POST, hTime);
  server.on("/api/v1/events", HTTP_GET, hEvents);
  server.onNotFound([] { ++reqCount; error(404, "unknown endpoint - see docs/API.md"); });
  server.begin();
  started = true;
  char why[30];
  snprintf(why, sizeof why, "start #%lu: %s", (unsigned long)bootCount(), resetReasonText());
  Events.add(EventType::Boot, -1, why);
  if (esp_reset_reason() == ESP_RST_BROWNOUT)
    Events.add(EventType::PowerWeak, -1, "power dip - volume limited");
  KC_LOG("API: listening on port %u", kPort);
}

void loop() {
  if (!started) return;
  server.handleClient();
  // mDNS follows the Wi-Fi connection.
  if (Net.connected() && !mdnsUp) {
    if (MDNS.begin(KC_HOSTNAME)) {
      MDNS.addService("kidsclock", "tcp", kPort);
      MDNS.addServiceTxt("kidsclock", "tcp", "api", "1");
      MDNS.addServiceTxt("kidsclock", "tcp", "fw", KC_FW_VERSION);
      MDNS.addService("http", "tcp", 80);
      mdnsUp = true;
      KC_LOG("API: mDNS up - %s.local, _kidsclock._tcp:%u", KC_HOSTNAME, kPort);
    }
  } else if (!Net.connected() && mdnsUp) {
    MDNS.end();
    mdnsUp = false;
  }
}

}  // namespace api
