// Daily routine checklist, stars & badges, routine reminder pop-up.
#include <math.h>
#include "Screens.h"
#include "UiHooks.h"
#include "../config/Config.h"
#include "../core/TimeLogic.h"
#include "../display/Theme.h"
#include "../display/Characters.h"
#include "../clock/ClockManager.h"
#include "../routine/RoutineManager.h"
#include "../settings/SettingsManager.h"

using namespace theme;

ReminderScreen ReminderPopup;

static const Sky& periodSky(uint8_t p) {
  switch (p) {
    case tl::PERIOD_MORNING: return SKY_MORNING;
    case tl::PERIOD_AFTERNOON: return SKY_AFTERNOON;
    case tl::PERIOD_EVENING: return SKY_EVENING;
    default: return SKY_NIGHT;
  }
}

static chars::Kind periodChar(uint8_t p) {
  switch (p) {
    case tl::PERIOD_MORNING: return chars::Kind::Sun;
    case tl::PERIOD_AFTERNOON: return chars::Kind::Bunny;
    case tl::PERIOD_EVENING: return chars::Kind::Kitty;
    default: return chars::Kind::Moon;
  }
}

// Stars bursting out from a point; t = 0..1 animation progress.
static void starBurst(Canvas& c, int cx, int cy, float t, bool rainbow) {
  static const uint32_t cols[] = {YELLOW_DEEP, PINK_DEEP, MINT_DEEP, BLUE_DEEP, LAV_DEEP, APRICOT};
  for (int i = 0; i < 12; ++i) {
    const float a = i * 0.5236f + t * 1.5f;
    const float r = 20 + t * 110;
    const int sz = (int)(10 - t * 5);
    if (sz > 1) draw::star(c, cx + (int)(cosf(a) * r), cy + (int)(sinf(a) * r * 0.8f), sz, rainbow ? cols[i % 6] : YELLOW_DEEP, t * 3);
  }
}

// ===========================================================================
// Routine checklist
// ===========================================================================
static constexpr int kRowH = 34;
static constexpr int kRowsY = 58;
static constexpr int kVisible = 4;

void RoutineScreen::load() {
  count_ = Routine.tasksIn((tl::DayPeriod)period_, rows_, KC_MAX_TASKS);
  if (sel_ > count_ + 1) sel_ = count_ + 1;
}

void RoutineScreen::onEnter() {
  const LocalTime t = Clock.now();
  period_ = t.valid ? tl::periodForHour(t.hour) : tl::PERIOD_MORNING;
  sel_ = 1;
  top_ = 0;
  celebrateUntil_ = 0;
  load();
  // Jump to the first job that is not done yet.
  for (int i = 0; i < count_; ++i) {
    if (!Routine.done(rows_[i])) { sel_ = i + 1; break; }
  }
  if (sel_ >= kVisible) top_ = sel_ - kVisible + 1;
}

void RoutineScreen::onEvent(const InputEvent& e) {
  const int n = count_ + 2;
  if (e.btn == Btn::A && e.type == Press::Long) {
    UI.click();
    UI.back();
    return;
  }
  if (e.btn == Btn::B) {
    if (e.type != Press::Short) return;
    if (sel_ == 0) {
      period_ = (period_ + 1) % tl::PERIOD_COUNT;
      load();
      top_ = 0;
      UI.click();
    } else if (sel_ == count_ + 1) {
      UI.click();
      UI.back();
    } else {
      const int task = rows_[sel_ - 1];
      if (Routine.done(task)) {
        Routine.undo(task);
        UI.click();
        UI.toast("Not done yet - that's OK!");
      } else {
        bool allDone = false;
        Routine.complete(task, &allDone);
        bigCelebration_ = allDone;
        celebrateUntil_ = millis() + (allDone ? 3500 : 1600);
        uihooks::celebrate(allDone);
        if (allDone) UI.toast("All done today! +3 bonus stars", 3500);
        if (sel_ < count_) ++sel_;  // move on to the next job
      }
    }
    return;
  }
  sel_ = (sel_ + (e.btn == Btn::A ? -1 : 1) + n) % n;
  const int listSel = sel_ == 0 ? 0 : sel_ - 1;
  if (listSel < top_) top_ = listSel;
  if (listSel >= top_ + kVisible) top_ = listSel - kVisible + 1;
  if (e.type == Press::Short) UI.click();
}

void RoutineScreen::render(Canvas& c, uint32_t now) {
  const Sky& sky = periodSky(period_);
  const bool dark = &sky == &SKY_NIGHT;
  draw::vgradient(c, 0, 0, KC_SCREEN_W, KC_SCREEN_H, sky.top, sky.bottom);

  // Period chip (selectable) + stars
  const bool chipSel = sel_ == 0;
  draw::card(c, 8, 6, 196, 30, 15, chipSel ? PINK_DEEP : SNOW, dark ? 0x1E2148u : SHADOW, 2);
  draw::iconArrow(c, 24, 21, 10, 3, chipSel ? SNOW : INK_SOFT);
  draw::iconArrow(c, 188, 21, 10, 1, chipSel ? SNOW : INK_SOFT);
  char title[24];
  snprintf(title, sizeof title, "%s jobs", tl::periodName((tl::DayPeriod)period_));
  draw::textC(c, title, 106, 22, draw::FONT_BODY, chipSel ? SNOW : INK);
  draw::card(c, 212, 6, 100, 30, 15, SNOW, dark ? 0x1E2148u : SHADOW, 2);
  draw::star(c, 234, 21, 10, YELLOW_DEEP);
  char st[12];
  snprintf(st, sizeof st, "%d", Routine.starsToday());
  draw::text(c, st, 250, 22, draw::FONT_TITLE, INK, lgfx::textdatum_t::middle_left);

  // Progress bar
  int done, total;
  Routine.progress((tl::DayPeriod)period_, done, total);
  c.fillSmoothRoundRect(10, 42, 300, 10, 5, dark ? 0x3B3F78u : SNOW);
  if (total > 0 && done > 0) c.fillSmoothRoundRect(10, 42, max(10, 300 * done / total), 10, 5, MINT_DEEP);

  // Rows: tasks + Back
  for (int r = 0; r < kVisible; ++r) {
    const int i = top_ + r;  // 0..count_ (count_ = Back)
    if (i > count_) break;
    const int y = kRowsY + r * kRowH;
    const bool sel = sel_ == i + 1;
    draw::card(c, 10, y, 300, kRowH - 5, 13, sel ? BLUSH : SNOW, dark ? 0x1E2148u : CREAM_DARK, 2);
    if (i == count_) {
      draw::iconArrow(c, 28, y + 14, 12, 3, INK);
      draw::text(c, "Back", 44, y + 15, draw::FONT_BODY, INK, lgfx::textdatum_t::middle_left);
      continue;
    }
    const int task = rows_[i];
    const RoutineTask& k = Routine.task(task);
    const bool isDone = Routine.done(task);
    if (isDone) {
      c.fillSmoothCircle(30, y + 14, 10, MINT_DEEP);
      draw::iconCheck(c, 30, y + 14, 12, SNOW);
    } else {
      c.fillSmoothCircle(30, y + 14, 10, 0xD9D2E0u);
      c.fillSmoothCircle(30, y + 14, 7, SNOW);
    }
    char name[KC_TASK_NAME_LEN + 2];
    draw::fitText(c, k.name, draw::FONT_BODY, 172, name, sizeof name);
    draw::text(c, name, 48, y + 15, draw::FONT_BODY, isDone ? INK_SOFT : INK, lgfx::textdatum_t::middle_left);
    if (isDone) {
      c.drawFastHLine(48, y + 15, draw::textW(c, name, draw::FONT_BODY), INK_SOFT);
      draw::star(c, 290, y + 14, 8, YELLOW_DEEP);
    } else if (k.minute >= 0) {
      char hm[12];
      tl::formatHM12AmPm(hm, sizeof hm, k.minute / 60, k.minute % 60);
      draw::text(c, hm, 300, y + 15, draw::FONT_SMALL, INK_SOFT, lgfx::textdatum_t::middle_right);
    }
  }
  if (count_ == 0) draw::textC(c, "No jobs here yet", 160, 100, draw::FONT_BODY, dark ? SNOW : INK_SOFT);

  // Celebration overlay
  if (celebrateUntil_) {
    const int32_t left = (int32_t)(celebrateUntil_ - now);
    if (left <= 0) {
      celebrateUntil_ = 0;
    } else {
      const float dur = bigCelebration_ ? 3500.f : 1600.f;
      const float t = 1.f - left / dur;
      starBurst(c, 160, 120, fmodf(t * (bigCelebration_ ? 3 : 1), 1.f), bigCelebration_);
      draw::card(c, 100, 76, 120, 92, 24, SNOW, SHADOW, 3);
      chars::draw(c, periodChar(period_), 160, 112, 22, now, chars::Mood::Proud, SNOW);
      draw::textC(c, bigCelebration_ ? "WOW!" : "+1 star", 160, 152, draw::FONT_BODY, PINK_DEEP);
    }
  }

  const char* mid = "OK";
  if (sel_ == 0) mid = "Next";
  else if (sel_ <= count_) mid = Routine.done(rows_[sel_ - 1]) ? "Undo" : "Done!";
  else mid = "Back";
  draw::hintBar(c, "^", mid, "v");
}

// ===========================================================================
// Stars & badges
// ===========================================================================
struct Badge { uint32_t at; const char* name; };
static const Badge kBadges[] = {{0, "Little Star"},       {10, "Bright Star"},     {25, "Super Helper"},
                                {50, "Routine Rocket"},   {100, "Clock Champion"}, {200, "Galaxy Hero"},
                                {400, "Universe Legend"}};
static constexpr int kBadgeCount = sizeof(kBadges) / sizeof(kBadges[0]);

void StarsScreen::onEvent(const InputEvent& e) {
  if (e.type == Press::Short || (e.btn == Btn::A && e.type == Press::Long)) {
    UI.click();
    UI.back();
  }
}

void StarsScreen::render(Canvas& c, uint32_t now) {
  draw::vgradient(c, 0, 0, KC_SCREEN_W, KC_SCREEN_H, 0xEDE4FFu, 0xFFE3EEu);
  // twinkles
  for (int i = 0; i < 8; ++i) {
    const int x = 20 + (i * 97) % 290, y = 14 + (i * 53) % 190;
    if (((now / 400) + i) % 3) draw::sparkle(c, x, y, 5, SNOW);
  }
  const uint32_t total = Routine.starsTotal();
  int level = 0;
  while (level + 1 < kBadgeCount && total >= kBadges[level + 1].at) ++level;

  // Big spinning star with today's count
  const float rot = sinf(now / 700.0f) * 0.15f;
  draw::star(c, 82, 92, 66, APRICOT, rot);
  draw::star(c, 82, 92, 60, YELLOW_DEEP, rot);
  char n[12];
  snprintf(n, sizeof n, "%d", Routine.starsToday());
  draw::textC(c, n, 82, 98, draw::FONT_BIG, SNOW);
  char today[32];
  if (AppCfg.get().childName[0]) snprintf(today, sizeof today, "%s today", AppCfg.get().childName);
  else snprintf(today, sizeof today, "today");
  draw::textC(c, today, 82, 168, draw::FONT_BODY, LAV_DEEP);

  draw::card(c, 160, 22, 150, 150, 20, SNOW, SHADOW, 3);
  draw::iconTrophy(c, 235, 52, 34, YELLOW_DEEP);
  draw::textC(c, kBadges[level].name, 235, 86, draw::FONT_BODY, PINK_DEEP);
  snprintf(n, sizeof n, "%lu stars", (unsigned long)total);
  draw::textC(c, n, 235, 108, draw::FONT_TITLE, INK);
  if (level + 1 < kBadgeCount) {
    const uint32_t from = kBadges[level].at, to = kBadges[level + 1].at;
    const int w = (int)(120 * (total - from) / (to - from));
    c.fillSmoothRoundRect(175, 128, 120, 10, 5, LAVENDER);
    if (w > 0) c.fillSmoothRoundRect(175, 128, max(10, w), 10, 5, LAV_DEEP);
    char next[32];
    snprintf(next, sizeof next, "%lu more to go!", (unsigned long)(to - total));
    draw::textC(c, next, 235, 154, draw::FONT_SMALL, INK_SOFT);
  } else {
    draw::textC(c, "Top badge!", 235, 140, draw::FONT_BODY, MINT_DEEP);
  }
  draw::hintBar(c, nullptr, "Back", nullptr);
}

// ===========================================================================
// Reminder pop-up
// ===========================================================================
void ReminderScreen::show(int task) {
  task_ = task;
  UI.go(ScreenId::Reminder);
  uihooks::reminderAlert();
}

void ReminderScreen::onEvent(const InputEvent& e) {
  if (e.type != Press::Short) return;
  UI.click();
  if (e.btn == Btn::A) {
    Routine.snoozeReminder(task_, 10);
    UI.back();
    UI.toast("I'll remind you in 10 minutes");
  } else if (e.btn == Btn::B) {
    bool allDone = false;
    Routine.complete(task_, &allDone);
    uihooks::celebrate(allDone);
    UI.back();
    UI.toast(allDone ? "All done today! +3 bonus stars" : "Great job! +1 star", 3000);
  } else {
    UI.back();
  }
}

void ReminderScreen::render(Canvas& c, uint32_t now) {
  if (task_ < 0) {
    UI.back();
    return;
  }
  const RoutineTask& k = Routine.task(task_);
  draw::vgradient(c, 0, 0, KC_SCREEN_W, KC_SCREEN_H, 0xD8F5EAu, 0xFFF4D6u);
  draw::card(c, 12, 14, 296, 150, 24, SNOW, SHADOW, 4);
  chars::draw(c, periodChar(k.period), 70, 84, 32, now, chars::Mood::Happy, SNOW);
  const int shake = (int)(sinf(now / 60.0f) * 3);
  draw::iconBell(c, 284 + shake, 36, 18, PINK_DEEP);
  char who[40];
  if (AppCfg.get().childName[0]) snprintf(who, sizeof who, "%s, time to...", AppCfg.get().childName);
  else snprintf(who, sizeof who, "Time to...");
  draw::text(c, who, 126, 44, draw::FONT_BODY, INK_SOFT);
  char name[KC_TASK_NAME_LEN + 2];
  draw::fitText(c, k.name, draw::FONT_TITLE, 172, name, sizeof name);
  draw::text(c, name, 126, 68, draw::FONT_TITLE, PINK_DEEP);
  if (k.minute >= 0) {
    char hm[12];
    tl::formatHM12AmPm(hm, sizeof hm, k.minute / 60, k.minute % 60);
    draw::text(c, hm, 126, 104, draw::FONT_BODY, INK);
  }
  draw::text(c, "Finished? Press Done!", 126, 130, draw::FONT_SMALL, INK_SOFT);
  draw::pill(c, theme::BTN_X_A - 44, 176, 88, 34, BUTTER, "Later", draw::FONT_BODY, INK, SHADOW);
  draw::pill(c, theme::BTN_X_B - 44, 176, 88, 34, MINT_DEEP, "Done!", draw::FONT_TITLE, SNOW, SHADOW);
  draw::pill(c, theme::BTN_X_C - 44, 176, 88, 34, LAVENDER, "OK", draw::FONT_BODY, INK, SHADOW);
}
