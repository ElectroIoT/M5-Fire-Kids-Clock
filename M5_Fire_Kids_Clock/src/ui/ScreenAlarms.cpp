// Alarm list, alarm editor and the ringing screen.
#include <math.h>
#include "Screens.h"
#include "UiHooks.h"
#include "../config/Config.h"
#include "../core/TimeLogic.h"
#include "../core/AlarmLogic.h"
#include "../display/Theme.h"
#include "../display/Characters.h"
#include "../alarm/AlarmManager.h"
#include "../audio/SoundList.h"
#include "../clock/ClockManager.h"
#include "../settings/SettingsManager.h"

using namespace theme;

AlarmEditScreen AlarmEditor;

static const char* const kLabels[] = {"Wake Up",      "School Wake Up", "Homework Time", "Bedtime",
                                      "Brush Teeth",  "Medicine",       "Reading Time",  "Play Time",
                                      "Lunch Time",   "Snack Time",     "Music Practice", "Tidy Up"};
static constexpr int kLabelCount = sizeof(kLabels) / sizeof(kLabels[0]);
// Monday-first display order of tm_wday bits.
static const int kDayOrder[7] = {1, 2, 3, 4, 5, 6, 0};
static const char* const kDayLetters[7] = {"M", "T", "W", "T", "F", "S", "S"};

static void toggleSwitch(Canvas& c, int x, int y, bool on) {
  c.fillSmoothRoundRect(x, y, 44, 22, 11, on ? MINT_DEEP : 0xD9D2E0u);
  c.fillSmoothCircle(on ? x + 33 : x + 11, y + 11, 8, SNOW);
}

// ===========================================================================
// List
// ===========================================================================
static constexpr int kListRowH = 40;
static constexpr int kListVisible = 4;

void AlarmListScreen::onEnter() {
  if (sel_ > KC_MAX_ALARMS) sel_ = 1;
}

void AlarmListScreen::onEvent(const InputEvent& e) {
  if (e.btn == Btn::A && e.type == Press::Long) {
    UI.click();
    UI.back();
    return;
  }
  if (e.btn == Btn::B) {
    if (sel_ == 0) {
      if (e.type == Press::Short) { UI.click(); UI.back(); }
      return;
    }
    al::AlarmConfig& a = Alarms.alarm(sel_ - 1);
    if (e.type == Press::Long) {  // quick on / off
      a.enabled ^= 1;
      Alarms.save();
      UI.click();
      UI.toast(a.enabled ? "Alarm on" : "Alarm off");
    } else if (e.type == Press::Short) {
      UI.click();
      AlarmEditor.edit(sel_ - 1);
    }
    return;
  }
  const int n = KC_MAX_ALARMS + 1;
  sel_ = (sel_ + (e.btn == Btn::A ? -1 : 1) + n) % n;
  if (sel_ < top_) top_ = sel_;
  if (sel_ >= top_ + kListVisible) top_ = sel_ - kListVisible + 1;
  if (e.type == Press::Short) UI.click();
}

void AlarmListScreen::render(Canvas& c, uint32_t now) {
  (void)now;
  c.fillScreen(CREAM);
  draw::header(c, "Alarms", BLUSH, INK);
  draw::iconBell(c, 30, 21, 16, PINK_DEEP);
  for (int r = 0; r < kListVisible; ++r) {
    const int i = top_ + r;
    if (i > KC_MAX_ALARMS) break;
    const int y = 44 + r * kListRowH;
    const bool sel = i == sel_;
    draw::card(c, 10, y, 296, kListRowH - 6, 14, sel ? BLUSH : SNOW, sel ? SHADOW : CREAM_DARK, 2);
    if (i == 0) {
      draw::iconArrow(c, 28, y + 17, 12, 3, INK);
      draw::text(c, "Back", 44, y + 17, draw::FONT_BODY, INK, lgfx::textdatum_t::middle_left);
      continue;
    }
    const al::AlarmConfig& a = Alarms.alarm(i - 1);
    const uint32_t ink = a.enabled ? INK : INK_SOFT;
    char hm[8];
    tl::formatHM12(hm, sizeof hm, a.hour, a.minute);
    draw::text(c, hm, 20, y + 4, draw::FONT_TITLE, ink);
    const int w = draw::textW(c, hm, draw::FONT_TITLE);
    draw::text(c, tl::ampm(a.hour), 24 + w, y + 6, draw::FONT_BODY, a.enabled ? PINK_DEEP : INK_SOFT);
    char days[24];
    al::describeDays(a, days, sizeof days);
    char shortLabel[al::LABEL_LEN + 2];
    draw::fitText(c, a.label, draw::FONT_BODY, 126, shortLabel, sizeof shortLabel);  // clear of the switch
    draw::text(c, shortLabel, 122, y + 3, draw::FONT_BODY, ink);
    draw::text(c, days, 122, y + 19, draw::FONT_SMALL, INK_SOFT);
    if (Alarms.snoozed(i - 1)) draw::text(c, "zz", 236, y + 19, draw::FONT_SMALL, LAV_DEEP);
    toggleSwitch(c, 254, y + 6, a.enabled);
  }
  // scroll bar
  const int n = KC_MAX_ALARMS + 1;
  const int trackH = kListVisible * kListRowH - 6;
  const int barH = trackH * kListVisible / n;
  c.fillSmoothRoundRect(311, 44, 5, trackH, 2, LAVENDER);
  c.fillSmoothRoundRect(311, 44 + (trackH - barH) * top_ / (n - kListVisible), 5, barH, 2, LAV_DEEP);
  draw::hintBar(c, "^", sel_ == 0 ? "Back" : "Edit", "v");
}

// ===========================================================================
// Editor
// ===========================================================================
void AlarmEditScreen::edit(int index) {
  index_ = index;
  const al::AlarmConfig& a = Alarms.alarm(index);
  tl::to12h(a.hour, hour12_, pm_);
  minute_ = a.minute;
  repeat_ = a.repeat;
  days_ = a.days ? a.days : al::DAYS_WEEKDAYS;
  sound_ = a.sound;
  enabled_ = 1;  // editing an alarm usually means "I want it"
  strlcpy(label_, a.label, sizeof label_);
  UI.go(ScreenId::AlarmEdit);
}

void AlarmEditScreen::onEnter() {
  field_ = F_HOUR;
  dayCursor_ = 0;
}

void AlarmEditScreen::commit() {
  al::AlarmConfig& a = Alarms.alarm(index_);
  a.hour = (uint8_t)tl::to24h(hour12_, pm_);
  a.minute = (uint8_t)minute_;
  a.repeat = repeat_;
  a.days = days_;
  a.sound = sound_;
  a.enabled = enabled_;
  strlcpy(a.label, label_, sizeof a.label);
  al::sanitize(a);
  Alarms.save();
  uihooks::stopSound();
  char msg[40], hm[12];
  tl::formatHM12AmPm(hm, sizeof hm, a.hour, a.minute);
  snprintf(msg, sizeof msg, a.enabled ? "Alarm set for %s" : "Saved (alarm is off)", hm);
  UI.back();
  UI.toast(msg);
}

void AlarmEditScreen::nextField() {
  uihooks::stopSound();
  field_ = (uint8_t)(field_ + 1);
  if (field_ == F_DAYS && repeat_ != al::REPEAT_CUSTOM) field_ = F_SOUND;
  if (field_ == F_DAYS) dayCursor_ = 0;
  if (field_ >= F_COUNT) field_ = F_SAVE;
}

void AlarmEditScreen::adjust(int dir) {
  switch (field_) {
    case F_HOUR: hour12_ += dir; if (hour12_ < 1) hour12_ = 12; if (hour12_ > 12) hour12_ = 1; break;
    case F_MIN: minute_ += dir; if (minute_ < 0) minute_ = 59; if (minute_ > 59) minute_ = 0; break;
    case F_AMPM: pm_ = !pm_; break;
    case F_REPEAT: repeat_ = (uint8_t)((repeat_ + dir + al::REPEAT_COUNT) % al::REPEAT_COUNT); break;
    case F_DAYS: dayCursor_ = (dayCursor_ + dir + 8) % 8; break;
    case F_SOUND:
      sound_ = (uint8_t)((sound_ + dir + sounds::ALARM_SOUND_COUNT) % sounds::ALARM_SOUND_COUNT);
      uihooks::previewSound(sound_);
      break;
    case F_LABEL: {
      int cur = -1;
      for (int i = 0; i < kLabelCount; ++i) if (!strcmp(kLabels[i], label_)) cur = i;
      cur = cur < 0 ? (dir > 0 ? 0 : kLabelCount - 1) : (cur + dir + kLabelCount) % kLabelCount;
      strlcpy(label_, kLabels[cur], sizeof label_);
      break;
    }
    case F_ON: enabled_ ^= 1; break;
    default: break;
  }
}

void AlarmEditScreen::onEvent(const InputEvent& e) {
  if (e.btn == Btn::A && e.type == Press::Long) {  // cancel
    uihooks::stopSound();
    UI.click();
    UI.back();
    UI.toast("Not changed");
    return;
  }
  if (e.btn == Btn::B) {
    if (e.type == Press::Long) { commit(); return; }  // save from anywhere
    if (e.type != Press::Short) return;
    UI.click();
    if (field_ == F_DAYS && dayCursor_ < 7) {
      days_ ^= (uint8_t)(1u << kDayOrder[dayCursor_]);
    } else if (field_ == F_SAVE) {
      commit();
    } else {
      nextField();
    }
    return;
  }
  adjust(e.btn == Btn::A ? -1 : 1);
  if (e.type == Press::Short && field_ != F_SOUND) UI.click();
}

static void editChip(Canvas& c, int x, int y, int w, const char* txt, bool focus) {
  draw::card(c, x, y, w, 40, 14, focus ? PINK_DEEP : SNOW, SHADOW, 3);
  draw::textC(c, txt, x + w / 2, y + 21, draw::FONT_TITLE, focus ? SNOW : INK);
}

void AlarmEditScreen::render(Canvas& c, uint32_t now) {
  (void)now;
  c.fillScreen(CREAM);
  char title[24];
  snprintf(title, sizeof title, "Alarm %d", index_ + 1);
  draw::header(c, title, BLUSH, INK);
  draw::iconBell(c, 30, 21, 16, PINK_DEEP);

  char b[8];
  snprintf(b, sizeof b, "%02d", hour12_);
  editChip(c, 50, 44, 62, b, field_ == F_HOUR);
  draw::textC(c, ":", 124, 63, draw::FONT_TITLE, INK);
  snprintf(b, sizeof b, "%02d", minute_);
  editChip(c, 136, 44, 62, b, field_ == F_MIN);
  editChip(c, 212, 44, 62, pm_ ? "PM" : "AM", field_ == F_AMPM);

  auto row = [&](int y, Field f, const char* label) {
    const bool focus = field_ == f;
    if (focus) c.fillSmoothRoundRect(8, y - 1, 304, 22, 11, BLUSH);
    draw::text(c, label, 18, y + 10, draw::FONT_BODY, focus ? INK : INK_SOFT, lgfx::textdatum_t::middle_left);
    return focus;
  };
  const uint32_t valInk = INK;
  int y = 94;
  bool f = row(y, F_REPEAT, "Repeat");
  draw::text(c, al::repeatName(repeat_), 300, y + 10, draw::FONT_BODY, f ? PINK_DEEP : valInk, lgfx::textdatum_t::middle_right);

  y += 23;
  f = row(y, F_DAYS, "Days");
  al::AlarmConfig tmp{};
  tmp.repeat = repeat_;
  tmp.days = days_;
  const uint8_t eff = al::effectiveDays(tmp);
  for (int i = 0; i < 7; ++i) {
    const int cx = 128 + i * 24, cy = y + 10;
    const bool on = (eff >> kDayOrder[i]) & 1;
    const bool cur = f && dayCursor_ == i;
    if (cur) c.fillSmoothCircle(cx, cy, 11, PINK_DEEP);
    c.fillSmoothCircle(cx, cy, 9, on ? (repeat_ == al::REPEAT_CUSTOM ? MINT_DEEP : 0xB9E4D4u) : 0xE6DFEAu);
    draw::textC(c, kDayLetters[i], cx, cy + 1, draw::FONT_SMALL, on && repeat_ == al::REPEAT_CUSTOM ? SNOW : INK);
  }
  if (f) {
    const bool cur = dayCursor_ == 7;
    draw::pill(c, 268, y, 38, 20, cur ? PINK_DEEP : SNOW, "OK", draw::FONT_SMALL, cur ? SNOW : INK);
  }

  y += 23;
  f = row(y, F_SOUND, "Sound");
  draw::text(c, sounds::alarmSoundName(sound_), 300, y + 10, draw::FONT_BODY, f ? PINK_DEEP : valInk, lgfx::textdatum_t::middle_right);
  y += 23;
  f = row(y, F_LABEL, "Name");
  draw::text(c, label_, 300, y + 10, draw::FONT_BODY, f ? PINK_DEEP : valInk, lgfx::textdatum_t::middle_right);
  y += 23;
  f = row(y, F_ON, "Alarm");
  toggleSwitch(c, 256, y - 1, enabled_);

  if (field_ == F_SAVE) {
    draw::hintBar(c, "Back", "Save", nullptr);
  } else if (field_ == F_DAYS) {
    draw::hintBar(c, "<", dayCursor_ < 7 ? "On/Off" : "Next", ">");
  } else {
    draw::hintBar(c, "-", "Next", "+");
  }
}

// ===========================================================================
// Ringing
// ===========================================================================
void RingScreen::onEvent(const InputEvent& e) {
  if (!Alarms.ringing()) {
    UI.home();
    return;
  }
  if (e.btn == Btn::A && (e.type == Press::Short || e.type == Press::Long)) {
    Alarms.snooze();
    char msg[32];
    snprintf(msg, sizeof msg, "Snoozing %d minutes", AppCfg.get().snoozeMin);
    UI.home();
    UI.toast(msg, 3000);
  } else if ((e.btn == Btn::B || e.btn == Btn::C) && e.type == Press::Short) {
    Alarms.stop();
    UI.home();
    UI.toast("Good job! Alarm stopped", 3000);
  }
}

void RingScreen::render(Canvas& c, uint32_t now) {
  const bool flash = (now / 500) % 2;
  draw::vgradient(c, 0, 0, KC_SCREEN_W, KC_SCREEN_H, flash ? 0xFFE0EBu : 0xFFF2C2u, flash ? 0xFFF2C2u : 0xFFE0EBu);
  // Bursting rays behind the character
  const float rot = now / 900.0f;
  for (int i = 0; i < 10; ++i) {
    const float a = rot + i * 0.628f;
    c.fillTriangle(70, 112, 70 + (int)(cosf(a) * 120), 112 + (int)(sinf(a) * 120), 70 + (int)(cosf(a + 0.25f) * 120),
                   112 + (int)(sinf(a + 0.25f) * 120), flash ? 0xFFEFC9u : 0xFFE5EEu);
  }
  const LocalTime t = Clock.now();
  chars::draw(c, chars::forHour(t.hour) == chars::Kind::Moon ? chars::Kind::Sun : chars::forHour(t.hour), 70, 112, 38,
              now, chars::Mood::Excited, SNOW);
  // Shaking bell
  const int shake = (int)(sinf(now / 40.0f) * 4);
  draw::iconBell(c, 296 + shake, 22, 22, PINK_DEEP);

  const int idx = Alarms.ringingIndex();
  const char* label = idx >= 0 ? Alarms.alarm(idx).label : "Alarm";
  char fitted[al::LABEL_LEN + 2];
  draw::fitText(c, label, draw::FONT_TITLE, 150, fitted, sizeof fitted);  // clear of the bell
  draw::text(c, fitted, 140, 34, draw::FONT_TITLE, PINK_DEEP);
  char hm[8];
  tl::formatHM12(hm, sizeof hm, t.hour, t.minute);
  draw::text(c, hm, 140, 62, draw::FONT_HUGE, INK);
  const int w = draw::textW(c, hm, draw::FONT_HUGE);
  draw::text(c, tl::ampm(t.hour), 146 + w, 72, draw::FONT_TITLE, PINK_DEEP);
  const char* nm = AppCfg.get().childName;
  char msg[40];
  if (Alarms.ringingIsSnooze()) snprintf(msg, sizeof msg, "Snooze is over!");
  else if (t.hour >= 4 && t.hour < 11) snprintf(msg, sizeof msg, nm[0] ? "Wake up, %s!" : "Time to get up!", nm);
  else snprintf(msg, sizeof msg, nm[0] ? "It's time, %s!" : "It's time!", nm);
  draw::text(c, msg, 140, 120, draw::FONT_BODY, INK_SOFT);

  // Big buttons above the physical keys
  char sn[16];
  snprintf(sn, sizeof sn, "Snooze %d", AppCfg.get().snoozeMin);
  const int pulse = flash ? 2 : 0;
  draw::pill(c, theme::BTN_X_A - 48, 172 - pulse, 96, 40, BUTTER, sn, draw::FONT_BODY, INK, SHADOW);
  draw::pill(c, theme::BTN_X_B - 44, 172 - pulse, 88, 40, RED_SOFT, "STOP", draw::FONT_TITLE, SNOW, SHADOW);
  draw::pill(c, theme::BTN_X_C - 44, 172 - pulse, 88, 40, RED_SOFT, "STOP", draw::FONT_TITLE, SNOW, SHADOW);
  draw::iconArrow(c, theme::BTN_X_A, 226, 10, 2, INK_SOFT);
  draw::iconArrow(c, theme::BTN_X_B, 226, 10, 2, INK_SOFT);
  draw::iconArrow(c, theme::BTN_X_C, 226, 10, 2, INK_SOFT);
}
