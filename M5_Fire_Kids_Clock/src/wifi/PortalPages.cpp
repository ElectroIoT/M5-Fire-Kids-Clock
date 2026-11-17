// Alarm and routine pages for the parent setup site.
#include "PortalPages.h"
#include <WebServer.h>
#include "../config/Config.h"
#include "../core/TimeLogic.h"
#include "../core/AlarmLogic.h"
#include "../alarm/AlarmManager.h"
#include "../audio/SoundList.h"
#include "../settings/SettingsManager.h"
#include "PortalRoutine.h"

namespace PortalPages {

static void pageAlarms() {
  web::begin("Alarms");
  web::send("<form method='post' action='/alarms'>");
  static const char* const dn[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
  for (int i = 0; i < KC_MAX_ALARMS; ++i) {
    const al::AlarmConfig& a = Alarms.alarm(i);
    char k[8];
    web::sendf("<div class='card'><h2>Alarm %d</h2>", i + 1);
    web::sendf("<label><input type='checkbox' name='e%d' value='1'%s> On</label>", i, a.enabled ? " checked" : "");
    web::send("<label>Name</label>");
    web::sendf("<input type='text' maxlength='%d' name='l%d' value='", al::LABEL_LEN - 1, i);
    web::sendEscaped(a.label);
    web::send("'><label>Time</label>");
    snprintf(k, sizeof k, "t%d", i);
    web::timePicker(k, a.hour, a.minute);
    web::sendf("<label>Repeat</label><select name='r%d'>", i);
    for (int r = 0; r < al::REPEAT_COUNT; ++r)
      web::sendf("<option value='%d'%s>%s</option>", r, r == a.repeat ? " selected" : "", al::repeatName(r));
    web::send("</select><div class='row'>");
    for (int d = 0; d < 7; ++d)
      web::sendf("<label style='font-weight:400'><input type='checkbox' name='d%d_%d' value='1'%s>%s</label>", i, d,
                 (a.days >> d) & 1 ? " checked" : "", dn[d]);
    web::send("</div><p class='muted'>Days only count when Repeat is \"Pick days\".</p>");
    web::sendf("<label>Sound</label><select name='s%d'>", i);
    for (int s = 0; s < sounds::ALARM_SOUND_COUNT; ++s)
      web::sendf("<option value='%d'%s>%s</option>", s, s == a.sound ? " selected" : "", sounds::alarmSoundName(s));
    web::send("</select></div>");
  }
  web::send("<button>Save alarms</button></form>");
  web::end();
}

static void postAlarms() {
  for (int i = 0; i < KC_MAX_ALARMS; ++i) {
    char k[12];
    al::AlarmConfig& a = Alarms.alarm(i);
    snprintf(k, sizeof k, "t%d", i);
    const int mod = web::readTime(k);
    if (mod < 0) continue;  // row not in the form
    a.hour = (uint8_t)(mod / 60);
    a.minute = (uint8_t)(mod % 60);
    snprintf(k, sizeof k, "e%d", i);
    a.enabled = web::has(k) ? 1 : 0;
    snprintf(k, sizeof k, "l%d", i);
    strlcpy(a.label, web::arg(k).c_str(), sizeof a.label);
    if (!a.label[0]) snprintf(a.label, sizeof a.label, "Alarm %d", i + 1);
    snprintf(k, sizeof k, "r%d", i);
    a.repeat = (uint8_t)web::argInt(k, a.repeat);
    a.days = 0;
    for (int d = 0; d < 7; ++d) {
      snprintf(k, sizeof k, "d%d_%d", i, d);
      if (web::has(k)) a.days |= (uint8_t)(1u << d);
    }
    snprintf(k, sizeof k, "s%d", i);
    a.sound = (uint8_t)web::argInt(k, a.sound);
    al::sanitize(a);
  }
  Alarms.save();
  KC_LOG("Portal: alarms saved");
  web::markSaved();
  web::redirect("/alarms");
}

void registerRoutes(WebServer& server) {
  server.on("/alarms", HTTP_GET, pageAlarms);
  server.on("/alarms", HTTP_POST, postAlarms);
  routine::registerRoutes(server);
}

void homeExtras() {
  int idx, mins, h, m;
  bool sn;
  if (Alarms.next(idx, mins, h, m, sn)) {
    char hm[12];
    tl::formatHM12AmPm(hm, sizeof hm, h, m);
    web::sendf("<div class='card'><h2>Next alarm</h2><p><b>%s</b> &middot; ", hm);
    web::sendEscaped(Alarms.alarm(idx).label);
    web::sendf(" (in %d h %d min)</p></div>", mins / 60, mins % 60);
  }
  routine::homeExtras();
}

void settingsExtras() {
  const Settings& s = AppCfg.get();
  web::send("<div class='card'><h2>Alarms</h2><label>Snooze time</label><select name='snz'>");
  for (int v : {5, 10, 15}) web::sendf("<option%s>%d</option>", s.snoozeMin == v ? " selected" : "", v);
  web::sendf("</select> minutes<label><input type='checkbox' name='fade' value='1'%s> Gentle fade-in volume</label>",
             s.gradualVolume ? " checked" : "");
  web::sendf("<label>Volume (0-10)</label><input type='number' name='vol' min='0' max='10' value='%d'>", s.volume);
  web::send("</div>");
  routine::settingsExtras();
}

void settingsSave() {
  Settings& s = AppCfg.get();
  s.snoozeMin = (uint8_t)web::argInt("snz", s.snoozeMin);
  s.gradualVolume = web::has("fade") ? 1 : 0;
  s.volume = (uint8_t)constrain(web::argInt("vol", s.volume), 0, 10);
  routine::settingsSave();
}

}  // namespace PortalPages
