// Home screen: the animated day clock face, the night clock face, and the
// "time not set yet" face. Layout is designed for 320 x 240 landscape.
#include <ctype.h>
#include <math.h>
#include "Screens.h"
#include "UiHooks.h"
#include "../config/Config.h"
#include "../core/TimeLogic.h"
#include "../display/Theme.h"
#include "../display/Characters.h"
#include "../clock/ClockManager.h"
#include "../wifi/WiFiLink.h"
#include "../power/BatteryManager.h"
#include "../settings/SettingsManager.h"
#include "../app/NightMode.h"

using namespace theme;

namespace {

struct DockItem {
  const char* label;
  ScreenId target;
  bool sleep;  // "Sleep" starts night mode instead of opening a screen
};

const DockItem kDock[] = {
    {"Alarms", ScreenId::AlarmList, false},
    {"Routine", ScreenId::Routine, false},
    {"Stars", ScreenId::Stars, false},
    {"Settings", ScreenId::Settings, false},
    {"Sleep", ScreenId::Home, true},
};
constexpr int kDockCount = sizeof(kDock) / sizeof(kDock[0]);

bool dockAvailable(int i) { return kDock[i].sleep || UI.screen(kDock[i].target) != nullptr; }

const Sky& skyFor(int hour) {
  switch (tl::periodForHour(hour)) {
    case tl::PERIOD_MORNING: return SKY_MORNING;
    case tl::PERIOD_AFTERNOON: return SKY_AFTERNOON;
    case tl::PERIOD_EVENING: return SKY_EVENING;
    default: return SKY_NIGHT;
  }
}

// Fixed pseudo-random star field.
void starField(Canvas& c, uint32_t now, int count, uint32_t bright, uint32_t dim, int maxY) {
  uint32_t seed = 0x2545F491u;
  for (int i = 0; i < count; ++i) {
    seed = seed * 1664525u + 1013904223u;
    const int x = 6 + (int)(seed % 308);
    seed = seed * 1664525u + 1013904223u;
    const int y = 4 + (int)(seed % (uint32_t)maxY);
    const bool on = ((now / 700) + i * 3) % 7 != 0;
    if (i % 4 == 0) draw::sparkle(c, x, y, on ? 4 : 3, on ? bright : dim);
    else c.fillSmoothCircle(x, y, 1, on ? bright : dim);
  }
}

}  // namespace

void HomeScreen::onEnter() {
  staticDirty_ = true;
  for (int n = 0; n < kDockCount && !dockAvailable(dockIndex_); ++n) dockIndex_ = (dockIndex_ + 1) % kDockCount;
}

uint32_t HomeScreen::frameMs() const {
  return NightMode.active() ? KC_FRAME_MS_NIGHT : KC_FRAME_MS_ANIM;
}

void HomeScreen::onEvent(const InputEvent& e) {
  staticDirty_ = true;
  if (NightMode.active()) {
    if (e.btn == Btn::C && e.type == Press::Long) {
      NightMode.skipTonight();
      UI.toast("Night mode off till tomorrow");
    } else {
      NightMode.peek();  // any press: show the bright clock for a moment
    }
    return;
  }
  if (NightMode.peeking()) NightMode.peek();  // keep peeking while used

  if (e.type == Press::Long && e.btn == Btn::C && NightMode.peeking()) {
    NightMode.skipTonight();
    NightMode.endPeek();
    UI.toast("Night mode off till tomorrow");
    return;
  }
  if (e.type != Press::Short) return;
  if (e.btn == Btn::A || e.btn == Btn::C) {
    const int dir = e.btn == Btn::A ? -1 : 1;
    for (int n = 0; n < kDockCount; ++n) {
      dockIndex_ = (dockIndex_ + dir + kDockCount) % kDockCount;
      if (dockAvailable(dockIndex_)) break;
    }
    UI.click();
  } else {
    UI.click();
    const DockItem& d = kDock[dockIndex_];
    if (d.sleep) {
      NightMode.forceNow(true);
      UI.toast("Good night!");
    } else {
      UI.go(d.target);
    }
  }
}

void HomeScreen::render(Canvas& c, uint32_t now) {
  if (NightMode.active()) renderNight(c, now);
  else if (!Clock.isValid()) renderNotSet(c, now);
  else renderDay(c, now);
}

void HomeScreen::statusBar(Canvas& c, bool night) {
  const uint32_t ink = night ? NIGHT_DIM : INK;
  const uint32_t off = night ? NIGHT_BG2 : 0xC8BFD0u;

  // Left: weather
  uint8_t kind; int temp; bool day;
  if (uihooks::weather(kind, temp, day)) {
    if (!night) draw::iconWeather(c, 16, 12, 22, kind, day);
    // The 7-bit FreeSans fonts have no degree glyph: draw a small ring instead.
    char t[12];
    snprintf(t, sizeof t, "%d", temp);
    const int x = night ? 8 : 32;
    draw::text(c, t, x, 4, draw::FONT_BODY, ink);
    const int w = draw::textW(c, t, draw::FONT_BODY);
    c.drawCircle(x + w + 4, 7, 2, ink);
    draw::text(c, "C", x + w + 8, 4, draw::FONT_BODY, ink);
  }

  // Centre: stars earned today
  int today, total;
  if (!night && uihooks::stars(today, total)) {
    draw::star(c, 146, 11, 8, YELLOW_DEEP);
    char s[8];
    snprintf(s, sizeof s, "%d", today);
    draw::text(c, s, 158, 4, draw::FONT_BODY, ink);
  }

  // Right: sync, Wi-Fi, battery
  const Settings& st = AppCfg.get();
  int x = 290;
  if (night) {
    // Night: only a dim battery warning, nothing else that glows.
    if (Battery.low()) draw::iconBattery(c, 8, 220, Battery.level(), false, NIGHT_DIM, NIGHT_DIM);
    return;
  }
  draw::iconBattery(c, x, 4, Battery.level(), Battery.charging(), ink, MINT_DEEP);
  x -= 20;
  if (st.wifiEnabled) {
    draw::iconWifi(c, x, 18, Net.bars(), ink, off, !Net.connected() && Net.state() != WiFiLink::State::Connecting);
    x -= 22;
    const bool fresh = Clock.source() == TimeSource::Ntp;
    draw::iconSync(c, x, 11, 6, Clock.syncing() ? PINK_DEEP : (fresh ? ink : off), fresh || !st.showSyncWarning);
  }
}

// Day face = cached static layer (rebuilt at most once a second or after a
// button press) + the few animated parts drawn on top every frame. Keeps a
// frame at a few ms instead of ~140 ms of anti-aliased drawing.
void HomeScreen::renderDay(Canvas& c, uint32_t now) {
  static LGFX_Sprite bg(&M5.Display);
  static bool bgOk = false, bgTried = false;
  static uint32_t bgSec = UINT32_MAX;
  if (!bgTried) {
    bgTried = true;
    bg.setPsram(true);
    bg.setColorDepth(16);
    bgOk = bg.createSprite(KC_SCREEN_W, KC_SCREEN_H) != nullptr;
  }
  // Static layer refresh: every 5 s, on input, and immediately when the shown
  // minute changes (also after the clock is set / synced).
  const LocalTime lt = Clock.now();
  const uint32_t sec = (now / 5000) * 2000u + (uint32_t)(lt.hour * 60 + lt.minute);
  if (bgOk) {
    if (sec != bgSec || staticDirty_) {
      renderDayStatic(bg, now);
      bgSec = sec;
      staticDirty_ = false;
    }
    memcpy(c.getBuffer(), bg.getBuffer(), KC_SCREEN_W * KC_SCREEN_H * 2);
  } else {
    renderDayStatic(c, now);
  }

  const LocalTime t = Clock.now();
  const Sky& sky = skyFor(t.hour);
  const bool darkSky = &sky == &SKY_NIGHT;
  // Blinking colon (geometry must match renderDayStatic)
  if ((now % 1000) < 600) {
    draw::BigTimeStyle st{44, 72, 10, 24, 6.5f, sky.digit, darkSky ? 0x1E2148u : 0xE7D9E6u, 0, 4,
                          darkSky ? 0x1E2148u : 0xFFFFFFu, 3.0f};
    const int tw = draw::bigTimeWidth(st);
    const int x0 = (KC_SCREEN_W - (tw + 12 + 48)) / 2, y0 = 28;
    const int cx = x0 + st.digitW * 2 + st.gap + st.colonW / 2;
    const int dr = (int)(st.stroke * 1.05f);
    for (int k = 0; k < 2; ++k) {
      const int cy = y0 + st.digitH * (k ? 7 : 3) / 10;
      c.fillSmoothCircle(cx, cy + st.shadowDy, dr + (int)st.outlineW, st.shadow);
      c.fillSmoothCircle(cx, cy, dr + (int)st.outlineW, st.outline);
      c.fillSmoothCircle(cx, cy, dr, st.color);
    }
  }
  chars::draw(c, chars::forHour(t.hour), 52, 170, 26, now, chars::Mood::Happy, sky.bottom);
  draw::hintBar(c, "<", kDock[dockIndex_].label, ">");
}

void HomeScreen::renderDayStatic(Canvas& c, uint32_t now) {
  const LocalTime t = Clock.now();
  const Sky& sky = skyFor(t.hour);
  const bool darkSky = &sky == &SKY_NIGHT;

  draw::vgradient(c, 0, 0, KC_SCREEN_W, KC_SCREEN_H, sky.top, sky.bottom);
  if (darkSky) {
    starField(c, now, 18, 0xFFF4C8u, 0x8C88C0u, 120);
  } else {
    draw::cloud(c, 34, 132, 14, 0xFFFFFFu);
    draw::cloud(c, 292, 128, 11, 0xFFFFFFu);
  }
  draw::hill(c, 150, darkSky ? 0x5C6FA8u : GRASS, darkSky ? 0x4A5B92u : GRASS_DARK);

  statusBar(c, false);

  // Big 12-hour time + AM/PM chips
  draw::BigTimeStyle st{44, 72, 10, 24, 6.5f, sky.digit, darkSky ? 0x1E2148u : 0xFFFFFFu, 0, 4,
                        darkSky ? 0x1E2148u : 0xFFFFFFu, 3.0f};
  if (!darkSky) st.shadow = 0xE7D9E6u;
  const int tw = draw::bigTimeWidth(st);
  const int chipW = 48, total = tw + 12 + chipW;
  const int x0 = (KC_SCREEN_W - total) / 2, y0 = 28;
  // The anti-aliased digits are the most expensive thing on screen: render
  // them into a cached layer once per minute and only draw the colon live.
  static LGFX_Sprite layer(&M5.Display);
  static bool layerOk = false;
  static int keyMin = -1;
  static const Sky* keySky = nullptr;
  constexpr int pad = 4;
  const int lw = tw + pad * 2, lh = st.digitH + pad * 3;
  if (!layerOk) {
    layer.setPsram(true);
    layer.setColorDepth(16);
    layerOk = layer.createSprite(lw, lh) != nullptr;
  }
  const int key = t.hour * 60 + t.minute;
  if (layerOk) {
    if (key != keyMin || keySky != &sky) {
      draw::vgradientPart(layer, lw, lh, sky.top, sky.bottom, KC_SCREEN_H, y0 - pad);
      draw::bigTime(layer, pad, pad, t.hour, t.minute, false, st);
      keyMin = key;
      keySky = &sky;
    }
    layer.pushSprite(&c, x0 - pad, y0 - pad);
  } else {
    draw::bigTime(c, x0, y0, t.hour, t.minute, false, st);  // no PSRAM: draw directly
  }
  draw::amPmChips(c, x0 + tw + 12, y0 + 4, chipW, 30, t.hour >= 12, sky.digit, SNOW,
                  darkSky ? 0x3B3F78u : 0xFFFFFFu, darkSky ? 0x8C88C0u : 0xC9BCD6u);

  // Date: "Tuesday, 17 Nov 2026" with the weekday in colour.
  char wd[16], rest[24];
  snprintf(wd, sizeof wd, "%s,", tl::weekdayName(t.wday));
  snprintf(rest, sizeof rest, " %02d %s %04d", t.day, tl::monthShort(t.month), t.year);
  const int w1 = draw::textW(c, wd, draw::FONT_TITLE), w2 = draw::textW(c, rest, draw::FONT_TITLE);
  const int dx = (KC_SCREEN_W - w1 - w2) / 2;
  const uint32_t dateInk = darkSky ? 0xEDE7FFu : INK;
  draw::text(c, wd, dx, 110, draw::FONT_TITLE, darkSky ? BUTTER : sky.digit);
  draw::text(c, rest, dx + w1, 110, draw::FONT_TITLE, dateInk);


  // Info card
  draw::card(c, 104, 142, 208, 62, 16, SNOW, darkSky ? 0x3B4A80u : SHADOW);
  char tm[12] = {0}, label[28] = {0}, when[12] = {0};
  bool whenOnLine2 = false;
  if (uihooks::nextAlarm(tm, sizeof tm, label, sizeof label, when, sizeof when)) {
    draw::iconBell(c, 122, 160, 16, PINK_DEEP);
    draw::text(c, tm, 136, 150, draw::FONT_TITLE, INK);
    const int tw = draw::textW(c, tm, draw::FONT_TITLE), ww = draw::textW(c, when, draw::FONT_SMALL);
    whenOnLine2 = 136 + tw + 8 + ww > 304;
    if (!whenOnLine2) draw::text(c, when, 304, 153, draw::FONT_SMALL, INK_SOFT, lgfx::textdatum_t::top_right);
  } else {
    draw::iconBell(c, 122, 160, 16, 0xC9BCD6u);
    draw::text(c, "No alarms set", 136, 152, draw::FONT_BODY, INK_SOFT);
  }

  // Second line rotates every 5 s between useful messages.
  char line[40];
  uint32_t color = INK_SOFT;
  const Settings& s = AppCfg.get();
  int done, totalTasks, today, totalStars;
  const char* period = nullptr;
  if (Battery.low()) {
    snprintf(line, sizeof line, "Battery low - please charge");
    color = RED_DEEP;
  } else if (s.showSyncWarning && Clock.source() == TimeSource::Retained) {
    snprintf(line, sizeof line, "Time not checked online yet");
    color = APRICOT;
  } else {
    const int slot = (int)((now / 5000) % 3);
    if (slot == 1 && uihooks::routineProgress(done, totalTasks, period) && totalTasks > 0) {
      snprintf(line, sizeof line, "%s jobs %d/%d", period, done, totalTasks);
      color = MINT_DEEP;
    } else if (slot == 2 && uihooks::stars(today, totalStars)) {
      if (s.childName[0]) {
        snprintf(line, sizeof line, "%d stars today, %s!", today, s.childName);
        if (draw::textW(c, line, draw::FONT_BODY) > 182) snprintf(line, sizeof line, "%d stars, %s!", today, s.childName);
      }
      else if (today == 0) snprintf(line, sizeof line, "Earn stars today!");
      else snprintf(line, sizeof line, "%d star%s today - yay!", today, today == 1 ? "" : "s");
      color = 0xE0A100u;
    } else if (label[0] && uihooks::nextAlarm(tm, sizeof tm, label, sizeof label, when, sizeof when)) {
      if (whenOnLine2) {
        when[0] = (char)toupper((unsigned char)when[0]);
        snprintf(line, sizeof line, "%s - %s", when, label);
      } else {
        snprintf(line, sizeof line, "%s", label);
      }
    } else {
      chars::greetingFor(t.hour, s.childName, line, sizeof line);
      if (s.childName[0] && draw::textW(c, line, draw::FONT_BODY) > 182) {
        snprintf(line, sizeof line, "%s %s!", t.hour >= 21 || t.hour < 5 ? "Sleep well," : "Hi", s.childName);
      }
      color = LAV_DEEP;
    }
  }
  char fitted[44];
  draw::fitText(c, line, draw::FONT_BODY, 182, fitted, sizeof fitted);
  draw::text(c, fitted, 122, 178, draw::FONT_BODY, color);
}

void HomeScreen::renderNight(Canvas& c, uint32_t now) {
  const LocalTime t = Clock.now();
  draw::vgradient(c, 0, 0, KC_SCREEN_W, KC_SCREEN_H, NIGHT_BG, NIGHT_BG2);
  starField(c, now, 14, 0x55528Au, 0x2C2A55u, 200);

  chars::draw(c, chars::Kind::Moon, 262, 46, 24, now, chars::Mood::Sleepy, NIGHT_BG);
  statusBar(c, true);

  if (t.valid) {
    draw::BigTimeStyle st{40, 66, 9, 22, 5.5f, NIGHT_INK, 0, 0, 0, 0, 0.f};
    const int tw = draw::bigTimeWidth(st);
    const int x0 = 16, y0 = 84;
    draw::bigTime(c, x0, y0, t.hour, t.minute, true, st);
    draw::amPmChips(c, x0 + tw + 10, y0 + 3, 42, 28, t.hour >= 12, NIGHT_DIM, 0xA9A5D8u, NIGHT_BG2, NIGHT_DIM);
    char date[40];
    tl::formatDateLong(date, sizeof date, t.wday, t.year, t.month, t.day);
    draw::text(c, date, 18, 162, draw::FONT_BODY, NIGHT_DIM);
  }

  char tm[12], label[28], when[12];
  if (uihooks::nextAlarm(tm, sizeof tm, label, sizeof label, when, sizeof when)) {
    draw::iconBell(c, 26, 200, 14, NIGHT_DIM);
    char line[32];
    snprintf(line, sizeof line, "%s %s", tm, when);
    draw::text(c, line, 40, 192, draw::FONT_BODY, NIGHT_DIM);
  }
  draw::text(c, "press a button to peek", 312, 230, draw::FONT_SMALL, 0x2E2C5Cu, lgfx::textdatum_t::bottom_right);
}

void HomeScreen::renderNotSet(Canvas& c, uint32_t now) {
  draw::vgradient(c, 0, 0, KC_SCREEN_W, KC_SCREEN_H, SKY_MORNING.top, SKY_MORNING.bottom);
  draw::cloud(c, (int)((now / 70) % 420) - 50, 50, 24, 0xFFFFFFu);
  draw::hill(c, 150, GRASS, GRASS_DARK);
  statusBar(c, false);
  chars::draw(c, chars::Kind::Sun, 60, 96, 30, now, chars::Mood::Happy, SKY_MORNING.bottom);
  draw::card(c, 112, 40, 198, 120, 18, SNOW, SHADOW);
  char hello[32];
  if (AppCfg.get().childName[0]) snprintf(hello, sizeof hello, "Hi %s!", AppCfg.get().childName);
  else snprintf(hello, sizeof hello, "Hello!");
  draw::textC(c, hello, 211, 62, draw::FONT_TITLE, PINK_DEEP);
  draw::textC(c, "What time is it?", 211, 88, draw::FONT_BODY, INK);
  if (Clock.syncing()) {
    draw::textC(c, "Asking the internet...", 211, 114, draw::FONT_SMALL, INK_SOFT);
  } else {
    draw::textC(c, "Grown-ups: open", 211, 112, draw::FONT_SMALL, INK_SOFT);
    draw::textC(c, "Settings > Set time", 211, 132, draw::FONT_SMALL, INK_SOFT);
  }
  draw::text(c, "Time not set", 160, 176, draw::FONT_BODY, APRICOT, lgfx::textdatum_t::top_center);
  draw::hintBar(c, "<", kDock[dockIndex_].label, ">");
}
