#include "App.h"
#include <M5Unified.h>
#include "../config/Config.h"
#include "../core/TimeLogic.h"
#include "../storage/StorageManager.h"
#include "../settings/SettingsManager.h"
#include "../display/DisplayManager.h"
#include "../display/Theme.h"
#include "../display/Characters.h"
#include "../input/InputManager.h"
#include "../clock/ClockManager.h"
#include "../wifi/WiFiLink.h"
#include "../power/BatteryManager.h"
#include "../ui/UiManager.h"
#include "../ui/Screens.h"
#include "../ui/UiHooks.h"
#include "../alarm/AlarmManager.h"
#include "../core/AlarmLogic.h"
#include "../audio/AudioManager.h"
#include "../led/LEDManager.h"
#include "../routine/RoutineManager.h"
#include "../weather/WeatherManager.h"
#include "NightMode.h"
#include "SerialConsole.h"
#include "ConsoleExtras.h"

// ---- Screens -------------------------------------------------------------------
static HomeScreen sHome;
static SettingsScreen sSettings;
static WifiSetupScreen sWifiSetup;
static SetTimeScreen sSetTime;
static InfoScreen sInfo;
static AlarmListScreen sAlarmList;
static RingScreen sRing;
static RoutineScreen sRoutine;
static StarsScreen sStars;

// ---- UI hooks (Phase 1: clock-only build) ---------------------------------
namespace uihooks {
void click() { Audio.click(); }
void previewSound(uint8_t s) { Audio.preview(s); }
void stopSound() {
  if (!Alarms.ringing()) Audio.stop();
}

bool nextAlarm(char* time12, size_t tn, char* label, size_t ln, char* when, size_t wn) {
  int idx, mins, h, m;
  bool snooze;
  if (!Alarms.next(idx, mins, h, m, snooze)) return false;
  tl::formatHM12AmPm(time12, tn, h, m);
  strlcpy(label, Alarms.alarm(idx).label, ln);
  const LocalTime t = Clock.now();
  const int dayOffset = (t.hour * 60 + t.minute + mins) / 1440;
  if (snooze) snprintf(when, wn, "snooze");
  else if (dayOffset == 0) snprintf(when, wn, "today");
  else if (dayOffset == 1) snprintf(when, wn, "tomorrow");
  else snprintf(when, wn, "%s", tl::weekdayShort((t.wday + dayOffset) % 7));
  return true;
}
bool weather(uint8_t& kind, int& tempC, bool& day) { return Weather.get(kind, tempC, day); }
bool stars(int& today, int& total) {
  today = Routine.starsToday();
  total = (int)Routine.starsTotal();
  return true;
}
bool routineProgress(int& done, int& total, const char*& periodName) {
  const LocalTime t = Clock.now();
  if (!t.valid) return false;
  const tl::DayPeriod p = tl::periodForHour(t.hour);
  Routine.progress(p, done, total);
  periodName = tl::periodName(p);
  return total > 0;
}
void celebrate(bool big) {
  Audio.effect(big ? sounds::FX_REWARD : sounds::FX_TASK_DONE);
  Leds.flash(big ? LEDManager::Effect::Reward : LEDManager::Effect::TaskDone, big ? 4000 : 1500);
}
void reminderAlert() {
  Audio.effect(sounds::FX_NOTIFY);
  Leds.flash(LEDManager::Effect::Notify, 6000);
}
}  // namespace uihooks

static int ledOverride = -1;  // serial "led" test command
static uint32_t loopMaxUs = 0, loopMaxShownUs = 0, loopWindowMs = 0;  // performance stats

namespace console {
namespace extras {
void help() {
  Serial.println(F("  alarms                 list alarms\n"
                   "  alarm N on|off|H:MM AM|PM   change alarm N (1..10)\n"
                   "  ring N                 ring alarm N now (test)\n"
                   "  stop | snooze          act on the ringing alarm\n"
                   "  sound N                preview alarm sound N (0..7)\n"
                   "  fx N                   effect 0 click,1 task,2 reward,3 notify,4 morning,5 lullaby\n"
                   "  vol N                  volume 0..10\n"
                   "  led NAME               off|standby|sunrise|morning|alarm|notify|task|reward|night|auto\n"
                   "  routine                list today's jobs\n"
                   "  done N | undo N        tick / untick job N\n"
                   "  remind N               show the reminder for job N\n"
                   "  weather LAT LON | off  set location (Open-Meteo) or turn weather off"));
}
void status() {
  Serial.println("Alarms :");
  Alarms.printStatus(Serial);
  Serial.println("Routine:");
  Routine.printStatus(Serial);
  uint8_t k; int tc; bool d;
  if (Weather.get(k, tc, d)) Serial.printf("Weather: %d C kind %d (%lus old)\n", tc, k, (unsigned long)Weather.ageSec());
  else Serial.printf("Weather: none (%s)\n", Weather.lastError());
  Serial.printf("Loop   : worst %lu us in the last 10 s (", (unsigned long)loopMaxShownUs);
  for (int i = 0; i < 7; ++i)
    Serial.printf("%s %lu%s", app::sectionName(i), (unsigned long)app::sectionWorst(i), i < 6 ? ", " : ")\n");
}
bool exec(const char* cmd, char* args) {
  if (!strcmp(cmd, "alarms")) { Alarms.printStatus(Serial); return true; }
  if (!strcmp(cmd, "routine")) { Routine.printStatus(Serial); return true; }
  if (!strcmp(cmd, "weather")) {
    float lat, lon;
    Settings& s = AppCfg.get();
    if (args && sscanf(args, "%f %f", &lat, &lon) == 2) {
      s.latitude = lat;
      s.longitude = lon;
      s.weatherEnabled = 1;
      AppCfg.save();
      Weather.refreshSoon();
      Serial.println("OK fetching");
    } else if (args && !strcmp(args, "off")) {
      s.latitude = NAN;
      s.longitude = NAN;
      AppCfg.save();
      Serial.println("OK weather location cleared");
    } else {
      Serial.println("usage: weather 20.30 85.82 | weather off");
    }
    return true;
  }
  if (!strcmp(cmd, "done")) {
    bool all = false;
    const int n = Routine.complete(atoi(args ? args : "0") - 1, &all);
    if (n) uihooks::celebrate(all);
    Serial.printf("OK +%d\n", n);
    return true;
  }
  if (!strcmp(cmd, "undo")) { Routine.undo(atoi(args ? args : "0") - 1); Serial.println("OK"); return true; }
  if (!strcmp(cmd, "remind")) {
    const int i = atoi(args ? args : "0") - 1;
    if (i >= 0 && i < KC_MAX_TASKS && Routine.used(i)) { ReminderPopup.show(i); Serial.println("OK"); }
    else Serial.println("ERR no such job");
    return true;
  }
  if (!strcmp(cmd, "sound")) { Audio.preview((uint8_t)atoi(args ? args : "0")); Serial.println("OK"); return true; }
  if (!strcmp(cmd, "fx")) { Audio.effect((sounds::Effect)atoi(args ? args : "0")); Serial.println("OK"); return true; }
  if (!strcmp(cmd, "vol")) {
    AppCfg.get().volume = (uint8_t)constrain(atoi(args ? args : "6"), 0, 10);
    AppCfg.markDirty();
    Audio.applyVolume();
    Serial.println("OK");
    return true;
  }
  if (!strcmp(cmd, "led")) {
    static const char* const names[] = {"off", "standby", "sunrise", "morning", "alarm", "notify", "task", "reward", "night"};
    ledOverride = -1;
    for (int i = 0; i < 9; ++i)
      if (args && !strcmp(args, names[i])) ledOverride = i;
    Serial.println(ledOverride >= 0 || (args && !strcmp(args, "auto")) ? "OK" : "usage: led off|...|auto");
    return true;
  }
  if (!strcmp(cmd, "ring")) { Alarms.test(atoi(args ? args : "1") - 1); Serial.println("OK"); return true; }
  if (!strcmp(cmd, "stop")) { Alarms.stop(); Serial.println("OK"); return true; }
  if (!strcmp(cmd, "snooze")) { Alarms.snooze(); Serial.println("OK"); return true; }
  if (!strcmp(cmd, "alarm")) {
    int n = 0, h = 0, m = 0;
    char ap[4] = {0}, word[8] = {0};
    if (args && sscanf(args, "%d %d:%d %3s", &n, &h, &m, ap) == 4 && n >= 1 && n <= KC_MAX_ALARMS && h >= 1 && h <= 12 && m >= 0 && m < 60) {
      al::AlarmConfig& a = Alarms.alarm(n - 1);
      a.hour = (uint8_t)tl::to24h(h, ap[0] == 'P' || ap[0] == 'p');
      a.minute = (uint8_t)m;
      a.enabled = 1;
      Alarms.save();
      Serial.println("OK");
    } else if (args && sscanf(args, "%d %7s", &n, word) == 2 && n >= 1 && n <= KC_MAX_ALARMS) {
      Alarms.alarm(n - 1).enabled = !strcmp(word, "on");
      Alarms.save();
      Serial.println("OK");
    } else {
      Serial.println("usage: alarm 1 6:30 AM | alarm 1 on|off");
    }
    return true;
  }
  return false;
}
}  // namespace extras
}  // namespace console

namespace app {

static void splash() {
  Canvas& c = Gfx.canvas();
  draw::vgradient(c, 0, 0, KC_SCREEN_W, KC_SCREEN_H, theme::SKY_MORNING.top, theme::SKY_MORNING.bottom);
  draw::hill(c, 150, theme::GRASS, theme::GRASS_DARK);
  chars::draw(c, chars::Kind::Sun, 160, 92, 34, 0, chars::Mood::Proud, theme::SKY_MORNING.bottom);
  char title[32];
  if (AppCfg.get().childName[0]) snprintf(title, sizeof title, "Hi %s!", AppCfg.get().childName);
  else snprintf(title, sizeof title, "Kids Clock");
  draw::textC(c, title, 160, 172, draw::FONT_BIG, theme::PINK_DEEP);
  draw::textC(c, "M5 Fire  v" KC_FW_VERSION, 160, 204, draw::FONT_SMALL, theme::INK);
  Gfx.setBrightness(AppCfg.get().brightness);
  Gfx.push();
}

static void handleAlarmEvents() {
  const int e = Alarms.takeEvent();
  if (e > 0) {
    Audio.startAlarm(Alarms.alarm(Alarms.ringingIndex()).sound);
    NightMode.endPeek();
    UI.wake();
    UI.go(ScreenId::Ring, false);
  } else if (e < 0) {
    Audio.stop();
    if (UI.current() == ScreenId::Ring) UI.home();
  }
}

// Pick the LED mood for the moment: alarm > night light > sunrise / morning > off.
static void updateLeds(const LocalTime& t) {
  using E = LEDManager::Effect;
  const Settings& s = AppCfg.get();
  Leds.setNightCap(NightMode.scheduled());
  if (ledOverride >= 0) { Leds.setBase((E)ledOverride); return; }
  if (Alarms.ringing()) { Leds.setBase(E::Alarm); return; }
  if (NightMode.scheduled()) { Leds.setBase(s.ledNightGlow ? E::Night : E::Off); return; }
  E e = E::Off;
  if (t.valid && s.nightEnabled) {
    const int since = (t.hour * 60 + t.minute - s.nightEndMin + 1440) % 1440;
    if (since < 15) e = E::Sunrise;
    else if (since < 60) e = E::Morning;
  }
  Leds.setBase(e);
}

static void handleNightEdges() {
  const int edge = NightMode.takeEdge();
  if (!edge) return;
  const Settings& s = AppCfg.get();
  if (edge > 0) {
    if (s.nightWifiOff) Net.setSuspended(true);
    if (s.bedtimeMusic && !Alarms.ringing()) Audio.effect(sounds::FX_BEDTIME);
  } else {
    Net.setSuspended(false);
  }
  UI.invalidate();
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = KC_SERIAL_BAUD;
  cfg.internal_spk = true;
  cfg.internal_mic = false;  // the mic shares I2S0 with the speaker on the Fire
  cfg.internal_imu = true;
  cfg.led_brightness = 0;
  M5.begin(cfg);

  Serial.println();
  KC_LOG("==== " KC_APP_NAME " v" KC_FW_VERSION " (" __DATE__ " " __TIME__ ") ====");
  KC_LOG("Board id %d, PSRAM %u KB, flash %u MB, reset reason %d", (int)M5.getBoard(), (unsigned)(ESP.getPsramSize() / 1024),
         (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)), (int)esp_reset_reason());

  Storage.begin();
  AppCfg.begin();
  Gfx.begin();
  splash();
  Input.begin();
  Battery.begin();
  Clock.begin();
  Net.begin();
  Alarms.begin();
  Audio.begin();
  Leds.begin();
  Routine.begin();
  Weather.begin();

  UI.registerScreen(ScreenId::Home, &sHome);
  UI.registerScreen(ScreenId::Settings, &sSettings);
  UI.registerScreen(ScreenId::WifiSetup, &sWifiSetup);
  UI.registerScreen(ScreenId::SetTime, &sSetTime);
  UI.registerScreen(ScreenId::Info, &sInfo);
  UI.registerScreen(ScreenId::Confirm, &ConfirmDlg);
  UI.registerScreen(ScreenId::AlarmList, &sAlarmList);
  UI.registerScreen(ScreenId::AlarmEdit, &AlarmEditor);
  UI.registerScreen(ScreenId::Ring, &sRing);
  UI.registerScreen(ScreenId::Routine, &sRoutine);
  UI.registerScreen(ScreenId::Stars, &sStars);
  UI.registerScreen(ScreenId::Reminder, &ReminderPopup);
  UI.begin();

  console::begin();
  KC_LOG("Setup done, free heap %u, free PSRAM %u", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
}

// Per-section worst-case timing (shown by the "status" console command).
static const char* const kSecName[] = {"input", "clock+net", "alarms", "night+led", "routine", "audio+led", "ui"};
static uint32_t secMax[7] = {0}, secShown[7] = {0};
static inline void mark(int i, uint32_t& t) {
  const uint32_t n = micros();
  if (n - t > secMax[i]) secMax[i] = n - t;
  t = n;
}

const char* sectionName(int i) { return kSecName[i]; }
uint32_t sectionWorst(int i) { return secShown[i]; }

void loop() {
  const uint32_t t0 = micros();
  uint32_t ts = t0;
  M5.update();
  Input.update();
  console::loop();
  mark(0, ts);
  Clock.loop();
  Net.loop();
  Battery.loop();
  mark(1, ts);

  const LocalTime t = Clock.now();
  Alarms.loop();          // runs before the UI so a ring is never a frame late
  handleAlarmEvents();
  mark(2, ts);
  NightMode.loop(t);
  handleNightEdges();
  updateLeds(t);
  mark(3, ts);
  {
    // Reminders wait while an alarm rings, at night, or while a parent edits.
    const ScreenId cur = UI.current();
    const bool busy = Alarms.ringing() || NightMode.active() || cur == ScreenId::Ring || cur == ScreenId::Reminder ||
                      cur == ScreenId::AlarmEdit || cur == ScreenId::SetTime || cur == ScreenId::WifiSetup;
    Routine.loop(!busy);
    const int due = Routine.takeReminder();
    if (due >= 0) ReminderPopup.show(due);
  }
  mark(4, ts);
  Audio.loop();
  Leds.loop();
  AppCfg.loop();
  mark(5, ts);
  UI.loop();
  mark(6, ts);

  const uint32_t us = micros() - t0;
  if (us > loopMaxUs) loopMaxUs = us;
  if (millis() - loopWindowMs > 10000) {
    loopMaxShownUs = loopMaxUs;
    loopMaxUs = 0;
    memcpy(secShown, secMax, sizeof secShown);
    memset(secMax, 0, sizeof secMax);
    loopWindowMs = millis();
  }
  vTaskDelay(1);  // let lower-priority tasks run; keeps the loop cool
}

}  // namespace app
