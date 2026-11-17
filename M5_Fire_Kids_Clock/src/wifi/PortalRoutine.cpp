// Routine pages of the parent setup site: edit job names, times, reminders.
#include "PortalRoutine.h"
#include <WebServer.h>
#include "PortalPages.h"
#include "../config/Config.h"
#include "../core/TimeLogic.h"
#include "../routine/RoutineManager.h"
#include "../settings/SettingsManager.h"

namespace PortalPages {
namespace routine {

static void pageRoutine() {
  web::begin("Routine");
  int d, t;
  Routine.progressAll(d, t);
  web::sendf("<div class='card'><h2>Today</h2><p>%d of %d jobs done &middot; %d stars today &middot; %lu stars in total</p>"
             "<p class='muted'>Leave a name empty to remove a job. Spare rows can be used for new jobs.</p></div>",
             d, t, Routine.starsToday(), (unsigned long)Routine.starsTotal());
  web::send("<form method='post' action='/routine'><div class='card'><table><tr><th>On</th><th>Job</th><th>Part of day</th>"
            "<th>Time</th><th>Remind</th></tr>");
  for (int i = 0; i < KC_MAX_TASKS; ++i) {
    const RoutineTask& k = Routine.task(i);
    web::sendf("<tr><td><input type='checkbox' name='e%d' value='1'%s></td><td><input type='text' maxlength='%d' name='n%d' value='",
               i, k.enabled ? " checked" : "", KC_TASK_NAME_LEN - 1, i);
    web::sendEscaped(k.name);
    web::sendf("'></td><td><select name='p%d'>", i);
    for (int p = 0; p < tl::PERIOD_COUNT; ++p)
      web::sendf("<option value='%d'%s>%s</option>", p, p == k.period ? " selected" : "", tl::periodName((tl::DayPeriod)p));
    web::send("</select></td><td>");
    char key[8];
    snprintf(key, sizeof key, "t%d", i);
    const int m = k.minute >= 0 ? k.minute : 7 * 60;
    web::timePicker(key, m / 60, m % 60);
    web::sendf("</td><td><input type='checkbox' name='r%d' value='1'%s></td></tr>", i, k.remind ? " checked" : "");
  }
  web::send("</table></div><button>Save routine</button></form>");
  web::end();
}

static void postRoutine() {
  for (int i = 0; i < KC_MAX_TASKS; ++i) {
    char k[8];
    RoutineTask& t = Routine.task(i);
    snprintf(k, sizeof k, "n%d", i);
    if (!web::has(k)) continue;
    strlcpy(t.name, web::arg(k).c_str(), sizeof t.name);
    snprintf(k, sizeof k, "e%d", i);
    t.enabled = (web::has(k) && t.name[0]) ? 1 : 0;
    snprintf(k, sizeof k, "p%d", i);
    t.period = (uint8_t)constrain(web::argInt(k, t.period), 0, tl::PERIOD_COUNT - 1);
    snprintf(k, sizeof k, "t%d", i);
    const int m = web::readTime(k);
    t.minute = (int16_t)(m >= 0 ? m : -1);
    snprintf(k, sizeof k, "r%d", i);
    t.remind = web::has(k) ? 1 : 0;
  }
  Routine.saveConfig();
  KC_LOG("Portal: routine saved");
  web::markSaved();
  web::redirect("/routine");
}

void registerRoutes(WebServer& server) {
  server.on("/routine", HTTP_GET, pageRoutine);
  server.on("/routine", HTTP_POST, postRoutine);
}

void homeExtras() {
  int d, t;
  Routine.progressAll(d, t);
  web::sendf("<div class='card'><h2>Routine</h2><p>%d of %d jobs done today, %d stars today.</p></div>", d, t,
             Routine.starsToday());
}

void settingsExtras() {
  const Settings& s = AppCfg.get();
  web::send("<div class='card'><h2>Routine</h2>");
  web::sendf("<label><input type='checkbox' name='rrem' value='1'%s> Pop-up reminders at job times</label>",
             s.routineReminders ? " checked" : "");
  web::send("<label>New day starts at</label><select name='rrh'>");
  for (int h = 0; h < 24; ++h) {
    char hm[12];
    tl::formatHM12AmPm(hm, sizeof hm, h, 0);
    web::sendf("<option value='%d'%s>%s</option>", h, h == s.routineResetHour ? " selected" : "", hm);
  }
  web::send("</select></div>");
}

void settingsSave() {
  Settings& s = AppCfg.get();
  s.routineReminders = web::has("rrem") ? 1 : 0;
  s.routineResetHour = (uint8_t)constrain(web::argInt("rrh", s.routineResetHour), 0, 23);
}

}  // namespace routine
}  // namespace PortalPages
