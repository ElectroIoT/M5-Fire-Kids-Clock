#include "WebPortal.h"
#include <WebServer.h>
#include <WiFi.h>
#include <math.h>
#include "../config/Config.h"
#include "../core/TimeLogic.h"
#include "../settings/SettingsManager.h"
#include "../clock/ClockManager.h"
#include "WiFiLink.h"
#include "PortalPages.h"
#include "../weather/WeatherManager.h"

WebPortal Portal;
static WebServer* server = nullptr;

// ---------------------------------------------------------------------------
// Small HTML helpers (chunked output keeps RAM use low)
// ---------------------------------------------------------------------------
namespace web {

// Output is collected into ~1.4 KB pieces before going on the wire: hundreds of
// tiny chunks (one per <option>) made big pages take 7+ s. Also, an EMPTY chunk
// means "end of response" in HTTP chunked encoding, so empty strings must never
// be sent on their own (an empty field used to cut the page off).
static String out;

static void flush() {
  if (out.length()) server->sendContent(out);
  out = "";
}

static void put(const char* s) {
  out += s;
  if (out.length() > 1400) flush();
}

void begin(const char* title) {
  out = "";
  out.reserve(1600);
  server->setContentLength(CONTENT_LENGTH_UNKNOWN);
  server->send(200, "text/html; charset=utf-8", "");
  put("<!doctype html><html><head><meta charset='utf-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'><title>");
  send(title);
  put("</title><style>"
    "body{font-family:system-ui,sans-serif;background:#FFF8EE;color:#4A3F55;margin:0;padding:12px;max-width:760px;margin:auto}"
    "h1{font-size:22px;color:#E06C9F;margin:6px 0 2px}h2{font-size:17px;margin:0 0 8px;color:#6B5B95}"
    ".card{background:#fff;border-radius:16px;padding:14px;margin:12px 0;box-shadow:0 2px 8px #0001}"
    "nav a{display:inline-block;margin:4px 6px 4px 0;padding:8px 12px;border-radius:999px;background:#E8DFF5;color:#4A3F55;text-decoration:none;font-weight:600}"
    "label{display:block;margin:8px 0 2px;font-weight:600;font-size:14px}"
    "input,select{font-size:15px;padding:7px;border:1px solid #d9cfe6;border-radius:10px;box-sizing:border-box}"
    "input[type=text],input[type=password],input[type=number]{width:100%}"
    "button{background:#7FD8BE;border:0;border-radius:999px;padding:10px 20px;font-size:16px;font-weight:700;color:#234;margin-top:12px}"
    "table{border-collapse:collapse;width:100%}td,th{padding:4px;border-bottom:1px solid #eee;text-align:left;font-size:14px}"
    ".muted{color:#8a7f96;font-size:13px}.ok{color:#2a9d6f;font-weight:700}.row{display:flex;gap:6px;flex-wrap:wrap;align-items:center}"
    "</style></head><body><h1>&#9200; " KC_APP_NAME "</h1>"
    "<nav><a href='/'>Home</a><a href='/wifi'>Wi-Fi</a><a href='/alarms'>Alarms</a>"
    "<a href='/routine'>Routine</a><a href='/settings'>Settings</a></nav>");
}

void end() {
  put("<p class='muted'>Firmware " KC_FW_VERSION ". This page closes by itself after 15 minutes.</p></body></html>");
  flush();
  server->sendContent("");  // the one intentional empty chunk: end of response
}

void send(const char* s) { put(s); }
void send(const String& s) { put(s.c_str()); }

void sendf(const char* fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  put(buf);
}

void sendEscaped(const char* s) {
  for (const char* p = s; *p; ++p) {
    switch (*p) {
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '&': out += "&amp;"; break;
      case '\'': out += "&#39;"; break;
      case '"': out += "&quot;"; break;
      default: out += *p;
    }
  }
  if (out.length() > 1400) flush();
}

String arg(const char* name) { return server->arg(name); }
bool has(const char* name) { return server->hasArg(name); }
int argInt(const char* name, int def) { return server->hasArg(name) ? server->arg(name).toInt() : def; }

void redirect(const char* to) {
  server->sendHeader("Location", to, true);
  server->send(303, "text/plain", "");
}

// 12-hour time picker: <hour 1..12> : <minute> <AM/PM>  (never 24-hour)
void timePicker(const char* prefix, int hour24, int minute) {
  int h12; bool pm;
  tl::to12h(hour24, h12, pm);
  sendf("<span class='row'><select name='%sh'>", prefix);
  for (int h = 1; h <= 12; ++h) sendf("<option%s>%d</option>", h == h12 ? " selected" : "", h);
  sendf("</select>:<select name='%sm'>", prefix);
  for (int m = 0; m < 60; ++m) sendf("<option value='%d'%s>%02d</option>", m, m == minute ? " selected" : "", m);
  sendf("</select><select name='%sp'><option value='0'%s>AM</option><option value='1'%s>PM</option></select></span>",
        prefix, pm ? "" : " selected", pm ? " selected" : "");
}

// Reads a timePicker back; returns minute-of-day (24-hour internal) or -1.
int readTime(const char* prefix) {
  char k[16];
  snprintf(k, sizeof k, "%sh", prefix);
  if (!server->hasArg(k)) return -1;
  const int h12 = server->arg(k).toInt();
  snprintf(k, sizeof k, "%sm", prefix);
  const int m = server->arg(k).toInt();
  snprintf(k, sizeof k, "%sp", prefix);
  const bool pm = server->arg(k).toInt() == 1;
  if (h12 < 1 || h12 > 12 || m < 0 || m > 59) return -1;
  return tl::to24h(h12, pm) * 60 + m;
}

void markSaved() {
  Portal.markSaved();
  Net.touchPortal();
}

}  // namespace web

// ---------------------------------------------------------------------------
// Pages
// ---------------------------------------------------------------------------
static void pageHome() {
  Net.touchPortal();
  web::begin("Kids Clock");
  LocalTime t = Clock.now();
  char hm[12], date[40];
  tl::formatHM12AmPm(hm, sizeof hm, t.hour, t.minute);
  tl::formatDateLong(date, sizeof date, t.wday, t.year, t.month, t.day);
  web::send("<div class='card'><h2>Status</h2>");
  if (t.valid) web::sendf("<p><b>%s</b> &middot; %s (India time)</p>", hm, date);
  else web::send("<p><b>Time is not set yet.</b> Connect Wi-Fi or set it below.</p>");
  web::sendf("<p class='muted'>Time source: %s &middot; Wi-Fi: %s", Clock.sourceName(), Net.stateName());
  if (Net.connected()) web::sendf(" (%s, %d dBm)", Net.ip().toString().c_str(), Net.rssi());
  web::send("</p></div>");
  PortalPages::homeExtras();
  web::send("<div class='card'><h2>Set time by hand</h2><p class='muted'>Only needed without internet. "
            "Wi-Fi time sync overrides this.</p><form method='post' action='/time'>");
  web::sendf("<label>Date</label><input type='date' name='date' value='%04d-%02d-%02d' style='width:auto'>",
             t.valid ? t.year : 2026, t.valid ? t.month : 1, t.valid ? t.day : 1);
  web::send("<label>Time</label>");
  web::timePicker("t", t.valid ? t.hour : 7, t.valid ? t.minute : 0);
  web::send("<br><button>Set clock</button></form></div>");
  web::end();
}

static void postTime() {
  const String d = web::arg("date");
  const int mod = web::readTime("t");
  int y = 0, mo = 0, da = 0;
  if (mod >= 0 && sscanf(d.c_str(), "%d-%d-%d", &y, &mo, &da) == 3 && Clock.setManual(y, mo, da, mod / 60, mod % 60)) {
    web::markSaved();
  }
  web::redirect("/");
}

static void pageWifi() {
  Net.touchPortal();
  web::begin("Wi-Fi");
  web::send("<div class='card'><h2>Home Wi-Fi</h2><form method='post' action='/wifi'>");
  web::send("<label>Network name (SSID)</label><input type='text' name='ssid' list='nets' maxlength='32' value='");
  web::sendEscaped(AppCfg.wifi().ssid);
  web::send("'><datalist id='nets'>");
  const int n = WiFi.scanComplete();
  if (n > 0) {
    for (int i = 0; i < n && i < 20; ++i) {
      web::send("<option value='");
      web::sendEscaped(WiFi.SSID(i).c_str());
      web::send("'>");
    }
  }
  web::send("</datalist>");
  if (n > 0) {
    web::send("<p class='muted'>Nearby: ");
    for (int i = 0; i < n && i < 8; ++i) {
      web::sendEscaped(WiFi.SSID(i).c_str());
      web::sendf(" (%d)%s", WiFi.RSSI(i), i + 1 < n && i < 7 ? ", " : "");
    }
    web::send("</p>");
  } else {
    web::send("<p class='muted'>Scanning for networks... reload in a few seconds.</p>");
    if (n == WIFI_SCAN_FAILED) WiFi.scanNetworks(true);
  }
  web::send("<label>Password</label><input type='password' name='pass' maxlength='64' placeholder='(unchanged if empty)'>");
  web::send("<label>Backup network (optional)</label><input type='text' name='ssid2' list='nets' maxlength='32' value='");
  web::sendEscaped(AppCfg.wifi().ssid2);
  web::send("'><label>Backup password</label><input type='password' name='pass2' maxlength='64' placeholder='(unchanged if empty)'>");
  web::sendf("<label><input type='checkbox' name='en' value='1'%s> Use Wi-Fi (time sync + weather)</label>",
             AppCfg.get().wifiEnabled ? " checked" : "");
  web::send("<button>Save &amp; connect</button></form>");
  web::sendf("<p class='muted'>Status: %s</p></div>", Net.stateName());
  web::end();
}

static void postWifi() {
  WifiCreds& c = AppCfg.wifi();
  const String ssid = web::arg("ssid");
  const String pass = web::arg("pass");
  const bool changedNet = ssid != c.ssid;
  strlcpy(c.ssid, ssid.c_str(), sizeof c.ssid);
  if (pass.length() || changedNet) strlcpy(c.pass, pass.c_str(), sizeof c.pass);
  const String ssid2 = web::arg("ssid2"), pass2 = web::arg("pass2");
  const bool changed2 = ssid2 != c.ssid2;
  strlcpy(c.ssid2, ssid2.c_str(), sizeof c.ssid2);
  if (pass2.length() || changed2) strlcpy(c.pass2, pass2.c_str(), sizeof c.pass2);
  AppCfg.saveWifi();
  AppCfg.get().wifiEnabled = web::has("en") ? 1 : 0;
  AppCfg.save();
  KC_LOG("Portal: Wi-Fi settings saved (network '%s')", c.ssid);
  Net.reconnectNow();
  web::markSaved();
  web::redirect("/wifi");
}

static void pageSettings() {
  Net.touchPortal();
  const Settings& s = AppCfg.get();
  web::begin("Settings");
  web::send("<form method='post' action='/settings'><div class='card'><h2>Child</h2>"
            "<label>Child's name (shown in greetings)</label><input type='text' name='child' maxlength='15' value='");
  web::sendEscaped(s.childName);
  web::send("'></div><div class='card'><h2>Weather</h2>"
            "<p class='muted'>Uses Open-Meteo (free, no API key). Find your latitude/longitude on any map app.</p>");
  web::sendf("<label><input type='checkbox' name='wx' value='1'%s> Show weather</label>", s.weatherEnabled ? " checked" : "");
  web::send("<label>Place name (shown on clock)</label><input type='text' name='city' maxlength='23' value='");
  web::sendEscaped(s.city);
  web::send("'>");
  if (isnan(s.latitude)) web::send("<label>Latitude</label><input type='text' name='lat' placeholder='e.g. 20.2961'>");
  else web::sendf("<label>Latitude</label><input type='text' name='lat' value='%.4f'>", s.latitude);
  if (isnan(s.longitude)) web::send("<label>Longitude</label><input type='text' name='lon' placeholder='e.g. 85.8245'>");
  else web::sendf("<label>Longitude</label><input type='text' name='lon' value='%.4f'>", s.longitude);
  web::send("</div><div class='card'><h2>Night mode</h2>");
  web::sendf("<label><input type='checkbox' name='night' value='1'%s> Automatic night mode</label>", s.nightEnabled ? " checked" : "");
  web::send("<label>Starts</label>");
  web::timePicker("ns", s.nightStartMin / 60, s.nightStartMin % 60);
  web::send("<label>Ends</label>");
  web::timePicker("ne", s.nightEndMin / 60, s.nightEndMin % 60);
  web::sendf("<label><input type='checkbox' name='lull' value='1'%s> Play a lullaby at bedtime</label>", s.bedtimeMusic ? " checked" : "");
  web::sendf("<label><input type='checkbox' name='nwifi' value='1'%s> Turn Wi-Fi off at night (alarms still work)</label>", s.nightWifiOff ? " checked" : "");
  web::sendf("<label><input type='checkbox' name='glow' value='1'%s> Soft purple night light (LEDs)</label>", s.ledNightGlow ? " checked" : "");
  web::send("</div>");
  PortalPages::settingsExtras();
  web::send("<button>Save settings</button></form>");
  web::end();
}

static void postSettings() {
  Settings& s = AppCfg.get();
  strlcpy(s.childName, web::arg("child").c_str(), sizeof s.childName);
  s.weatherEnabled = web::has("wx") ? 1 : 0;
  strlcpy(s.city, web::arg("city").c_str(), sizeof s.city);
  const String lat = web::arg("lat"), lon = web::arg("lon");
  s.latitude = lat.length() ? lat.toFloat() : NAN;
  s.longitude = lon.length() ? lon.toFloat() : NAN;
  s.nightEnabled = web::has("night") ? 1 : 0;
  int v = web::readTime("ns");
  if (v >= 0) s.nightStartMin = v;
  v = web::readTime("ne");
  if (v >= 0) s.nightEndMin = v;
  s.bedtimeMusic = web::has("lull") ? 1 : 0;
  s.nightWifiOff = web::has("nwifi") ? 1 : 0;
  s.ledNightGlow = web::has("glow") ? 1 : 0;
  PortalPages::settingsSave();
  AppCfg.save();
  Weather.refreshSoon();
  KC_LOG("Portal: settings saved");
  web::markSaved();
  web::redirect("/settings");
}

static void notFound() {
  // Captive-portal style: send every unknown URL to the home page.
  String to = "http://" + WiFi.softAPIP().toString() + "/";
  server->sendHeader("Location", to, true);
  server->send(302, "text/plain", "");
}

void WebPortal::begin() {
  if (running_) return;
  if (!server) server = new WebServer(80);
  server->on("/", HTTP_GET, pageHome);
  server->on("/time", HTTP_POST, postTime);
  server->on("/wifi", HTTP_GET, pageWifi);
  server->on("/wifi", HTTP_POST, postWifi);
  server->on("/settings", HTTP_GET, pageSettings);
  server->on("/settings", HTTP_POST, postSettings);
  PortalPages::registerRoutes(*server);
  server->onNotFound(notFound);
  server->begin();
  running_ = true;
}

void WebPortal::end() {
  if (!running_) return;
  server->stop();
  delete server;  // drops all routes; a fresh server is built next time
  server = nullptr;
  running_ = false;
}

void WebPortal::loop() {
  if (running_ && server) server->handleClient();
}
