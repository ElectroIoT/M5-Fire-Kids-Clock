// Menu framework + system screens: Settings, Parent Setup, Set Time, Info, Confirm.
#include <WiFi.h>
#include <esp_system.h>
#include "Screens.h"
#include "SettingsMenu.h"
#include "../config/Config.h"
#include "../core/TimeLogic.h"
#include "../display/Theme.h"
#include "../display/Characters.h"
#include "../clock/ClockManager.h"
#include "../wifi/WiFiLink.h"
#include "../wifi/WebPortal.h"
#include "../power/BatteryManager.h"
#include "../settings/SettingsManager.h"

using namespace theme;

ConfirmScreen ConfirmDlg;

// ===========================================================================
// MenuScreen
// ===========================================================================
static constexpr int kRowH = 32;
static constexpr int kRowsTop = 44;
static constexpr int kVisibleRows = 5;

void MenuScreen::rebuild() {
  items_.clear();
  build(items_);
  if (sel_ >= (int)items_.size()) sel_ = (int)items_.size() - 1;
  if (sel_ < 0) sel_ = 0;
  while (sel_ < (int)items_.size() - 1 && items_[sel_].kind == MenuItem::Header) ++sel_;
}

void MenuScreen::onEnter() {
  editing_ = false;
  rebuild();
}

void MenuScreen::move(int dir) {
  const int n = (int)items_.size();
  if (!n) return;
  int s = sel_;
  for (int i = 0; i < n; ++i) {
    s = (s + dir + n) % n;
    if (items_[s].kind != MenuItem::Header) break;
  }
  sel_ = s;
  if (sel_ < top_) top_ = sel_;
  if (sel_ >= top_ + kVisibleRows) top_ = sel_ - kVisibleRows + 1;
  // Show the section header above the first item of a section when possible.
  if (sel_ == top_ && top_ > 0 && items_[top_ - 1].kind == MenuItem::Header) --top_;
}

void MenuScreen::onEvent(const InputEvent& e) {
  if (items_.empty()) return;
  MenuItem& it = items_[sel_];
  if (e.btn == Btn::A && e.type == Press::Long) {
    if (editing_) editing_ = false;
    else UI.back();
    UI.click();
    return;
  }
  if (editing_) {
    if (e.btn == Btn::B && e.type == Press::Short) {
      editing_ = false;
      UI.click();
    } else if (e.btn != Btn::B && it.adjust) {
      it.adjust(e.btn == Btn::A ? -1 : +1);
    }
    return;
  }
  if (e.btn == Btn::B) {
    if (e.type != Press::Short) return;
    UI.click();
    switch (it.kind) {
      case MenuItem::Action: if (it.action) it.action(); break;
      case MenuItem::Toggle: if (it.adjust) it.adjust(1); break;
      case MenuItem::Value: editing_ = true; break;
      default: break;
    }
    rebuild();  // items may appear / disappear (e.g. Wi-Fi off hides Wi-Fi items)
    return;
  }
  move(e.btn == Btn::A ? -1 : 1);
  if (e.type == Press::Short) UI.click();
}

void MenuScreen::render(Canvas& c, uint32_t now) {
  (void)now;
  c.fillScreen(CREAM);
  draw::header(c, title_, color_, INK);
  draw::iconGear(c, 30, 21, 8, INK, color_);

  const int n = (int)items_.size();
  for (int r = 0; r < kVisibleRows; ++r) {
    const int i = top_ + r;
    if (i >= n) break;
    const MenuItem& it = items_[i];
    const int y = kRowsTop + r * kRowH;
    if (it.kind == MenuItem::Header) {
      draw::text(c, it.label, 18, y + 12, draw::FONT_BODY, LAV_DEEP, lgfx::textdatum_t::middle_left);
      c.drawFastHLine(18 + draw::textW(c, it.label, draw::FONT_BODY) + 8, y + 13, 280 - draw::textW(c, it.label, draw::FONT_BODY), LAVENDER);
      continue;
    }
    const bool selected = i == sel_;
    const uint32_t fill = selected ? (editing_ ? BUTTER : BLUSH) : SNOW;
    draw::card(c, 10, y, 296, kRowH - 4, 12, fill, selected ? SHADOW : CREAM_DARK, 2);
    draw::text(c, it.label, 22, y + 15, draw::FONT_BODY, INK, lgfx::textdatum_t::middle_left);
    char v[32] = {0};
    if (it.value) it.value(v, sizeof v);
    if (it.kind == MenuItem::Action && !v[0]) {
      draw::iconArrow(c, 292, y + 14, 10, 1, selected ? PINK_DEEP : INK_SOFT);
    } else if (v[0]) {
      const uint32_t vc = selected ? (editing_ ? INK : PINK_DEEP) : INK_SOFT;
      if (selected && editing_) {
        const int w = draw::textW(c, v, draw::FONT_BODY);
        draw::iconArrow(c, 292 - w - 14, y + 14, 10, 3, PINK_DEEP);
        draw::iconArrow(c, 296, y + 14, 10, 1, PINK_DEEP);
        draw::text(c, v, 286, y + 15, draw::FONT_BODY, vc, lgfx::textdatum_t::middle_right);
      } else {
        draw::text(c, v, 296, y + 15, draw::FONT_BODY, vc, lgfx::textdatum_t::middle_right);
      }
    }
  }
  // Scroll bar
  if (n > kVisibleRows) {
    const int trackH = kVisibleRows * kRowH - 6;
    const int barH = max(16, trackH * kVisibleRows / n);
    const int barY = kRowsTop + (trackH - barH) * top_ / max(1, n - kVisibleRows);
    c.fillSmoothRoundRect(311, kRowsTop, 5, trackH, 2, LAVENDER);
    c.fillSmoothRoundRect(311, barY, 5, barH, 2, LAV_DEEP);
  }
  if (editing_) draw::hintBar(c, "-", "OK", "+");
  else draw::hintBar(c, "^", items_.empty() ? nullptr : (items_[sel_].kind == MenuItem::Value ? "Change" : "OK"), "v");
}

// ===========================================================================
// Settings
// ===========================================================================
void SettingsScreen::build(std::vector<MenuItem>& items) { settingsmenu::build(items); }

// ===========================================================================
// Parent setup
// ===========================================================================
void WifiSetupScreen::onEnter() {
  if (!Net.startPortal()) UI.toast("Could not start setup Wi-Fi");
}

void WifiSetupScreen::onEvent(const InputEvent& e) {
  if (e.type != Press::Short && !(e.btn == Btn::A && e.type == Press::Long)) return;
  if (e.btn == Btn::A || e.btn == Btn::B) {
    UI.click();
    Net.stopPortal();
    UI.back();
  }
}

void WifiSetupScreen::render(Canvas& c, uint32_t now) {
  (void)now;
  if (Portal.takeSavedFlag()) UI.toast("Saved!");
  c.fillScreen(CREAM);
  draw::header(c, "Parent Setup", MINT, INK);
  if (!Net.portalActive()) {
    draw::textC(c, "Setup Wi-Fi is off", 160, 110, draw::FONT_TITLE, INK_SOFT);
    draw::hintBar(c, "Back", nullptr, nullptr);
    return;
  }
  char qr[96];
  snprintf(qr, sizeof qr, "WIFI:T:WPA;S:%s;P:%s;;", Net.apSsid(), Net.apPass());
  c.fillSmoothRoundRect(8, 42, 128, 128, 12, SNOW);
  c.qrcode(qr, 14, 48, 116, 4);
  draw::textC(c, "scan to join", 72, 184, draw::FONT_SMALL, INK_SOFT);

  int y = 46;
  draw::text(c, "1. Join this Wi-Fi:", 146, y, draw::FONT_SMALL, INK_SOFT);
  draw::text(c, Net.apSsid(), 146, y + 18, draw::FONT_BODY, PINK_DEEP);
  draw::text(c, "Password:", 146, y + 40, draw::FONT_SMALL, INK_SOFT);
  draw::text(c, Net.apPass(), 146, y + 58, draw::FONT_TITLE, INK);
  draw::text(c, "2. Open in a browser:", 146, y + 88, draw::FONT_SMALL, INK_SOFT);
  char url[40];
  snprintf(url, sizeof url, "http://%s", Net.apIp().toString().c_str());
  draw::text(c, url, 146, y + 106, draw::FONT_BODY, BLUE_DEEP);
  if (Net.connected()) {
    snprintf(url, sizeof url, "or http://%s", Net.ip().toString().c_str());
    draw::text(c, url, 146, y + 128, draw::FONT_SMALL, INK_SOFT);
  }
  draw::hintBar(c, "Back", "Done", nullptr);
}

// ===========================================================================
// Set time & date
// ===========================================================================
static int monthFromBuildDate() {
  static const char* m = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char* d = __DATE__;  // "Nov 17 2026"
  for (int i = 0; i < 12; ++i) if (strncmp(d, m + i * 3, 3) == 0) return i + 1;
  return 1;
}

void SetTimeScreen::onEnter() {
  field_ = 0;
  LocalTime t = Clock.now();
  if (!t.valid) {
    year_ = atoi(__DATE__ + 7);
    month_ = monthFromBuildDate();
    day_ = atoi(__DATE__ + 4);
    t.hour = 7;
    t.minute = 0;
  } else {
    year_ = t.year;
    month_ = t.month;
    day_ = t.day;
  }
  tl::to12h(t.hour, hour12_, pm_);
  minute_ = t.minute;
  if (year_ < KC_MIN_VALID_YEAR) year_ = KC_MIN_VALID_YEAR;
}

void SetTimeScreen::adjust(int dir) {
  switch (field_) {
    case 0: day_ += dir; break;
    case 1: month_ += dir; if (month_ < 1) month_ = 12; if (month_ > 12) month_ = 1; break;
    case 2: year_ += dir; if (year_ < KC_MIN_VALID_YEAR) year_ = KC_MIN_VALID_YEAR; if (year_ > 2099) year_ = 2099; break;
    case 3: hour12_ += dir; if (hour12_ < 1) hour12_ = 12; if (hour12_ > 12) hour12_ = 1; break;
    case 4: minute_ += dir; if (minute_ < 0) minute_ = 59; if (minute_ > 59) minute_ = 0; break;
    case 5: pm_ = !pm_; break;
  }
  const int dim = tl::daysInMonth(year_, month_);
  if (day_ < 1) day_ = dim;
  if (day_ > dim) day_ = (field_ == 0) ? 1 : dim;
}

void SetTimeScreen::onEvent(const InputEvent& e) {
  if (e.btn == Btn::A && e.type == Press::Long) {
    UI.click();
    UI.back();
    return;
  }
  if (e.btn == Btn::B) {
    if (e.type != Press::Short) return;
    UI.click();
    if (field_ < 5) {
      ++field_;
    } else if (Clock.setManual(year_, month_, day_, tl::to24h(hour12_, pm_), minute_)) {
      UI.toast("Clock set!");
      UI.back();
    } else {
      UI.toast("That date is not valid");
    }
    return;
  }
  adjust(e.btn == Btn::A ? -1 : 1);
  if (e.type == Press::Short) UI.click();
}

static void chip(Canvas& c, int x, int y, int w, const char* txt, bool focus) {
  if (focus) {
    draw::card(c, x, y, w, 40, 14, PINK_DEEP, SHADOW, 3);
    draw::textC(c, txt, x + w / 2, y + 21, draw::FONT_TITLE, SNOW);
    draw::iconArrow(c, x + w / 2, y - 8, 10, 0, PINK_DEEP);
    draw::iconArrow(c, x + w / 2, y + 52, 10, 2, PINK_DEEP);
  } else {
    draw::card(c, x, y, w, 40, 14, SNOW, SHADOW, 3);
    draw::textC(c, txt, x + w / 2, y + 21, draw::FONT_TITLE, INK);
  }
}

void SetTimeScreen::render(Canvas& c, uint32_t now) {
  (void)now;
  c.fillScreen(CREAM);
  draw::header(c, "Set Time & Date", BUTTER, INK);
  char b[8];
  draw::text(c, "Date", 16, 70, draw::FONT_BODY, INK_SOFT, lgfx::textdatum_t::middle_left);
  snprintf(b, sizeof b, "%02d", day_);
  chip(c, 70, 52, 56, b, field_ == 0);
  chip(c, 134, 52, 70, tl::monthShort(month_), field_ == 1);
  snprintf(b, sizeof b, "%04d", year_);
  chip(c, 212, 52, 86, b, field_ == 2);

  draw::text(c, "Time", 16, 148, draw::FONT_BODY, INK_SOFT, lgfx::textdatum_t::middle_left);
  snprintf(b, sizeof b, "%02d", hour12_);
  chip(c, 70, 130, 56, b, field_ == 3);
  draw::textC(c, ":", 138, 149, draw::FONT_TITLE, INK);
  snprintf(b, sizeof b, "%02d", minute_);
  chip(c, 150, 130, 56, b, field_ == 4);
  chip(c, 222, 130, 64, pm_ ? "PM" : "AM", field_ == 5);

  char wd[32];
  snprintf(wd, sizeof wd, "That is a %s", tl::weekdayName(tl::weekdayOf(year_, month_, day_)));
  draw::textC(c, wd, 160, 196, draw::FONT_SMALL, INK_SOFT);
  draw::hintBar(c, "-", field_ < 5 ? "Next" : "Save", "+");
}

// ===========================================================================
// Device info
// ===========================================================================
void InfoScreen::onEvent(const InputEvent& e) {
  if (e.type == Press::Short || (e.btn == Btn::A && e.type == Press::Long)) {
    UI.click();
    UI.back();
  }
}

void InfoScreen::render(Canvas& c, uint32_t now) {
  (void)now;
  c.fillScreen(CREAM);
  draw::header(c, "Device Info", POWDER, INK);
  char v[48];
  int y = 44;
  auto row = [&](const char* k, const char* val) {
    draw::text(c, k, 14, y, draw::FONT_SMALL, INK_SOFT);
    draw::text(c, val, 112, y, draw::FONT_SMALL, INK);
    y += 19;
  };
  row("Firmware", KC_FW_VERSION " (" __DATE__ ")");
  snprintf(v, sizeof v, "M5Stack Fire, PSRAM %u KB", (unsigned)(ESP.getPsramSize() / 1024));
  row("Board", v);
  row("Time from", Clock.sourceName());
  if (Clock.lastSyncEpoch()) {
    const time_t ls = Clock.lastSyncEpoch();
    struct tm s;
    localtime_r(&ls, &s);
    char hm[12];
    tl::formatHM12AmPm(hm, sizeof hm, s.tm_hour, s.tm_min);
    snprintf(v, sizeof v, "%s, %02d %s", hm, s.tm_mday, tl::monthShort(s.tm_mon + 1));
  } else {
    snprintf(v, sizeof v, "never");
  }
  row("Last sync", v);
  if (Net.connected()) snprintf(v, sizeof v, "%s (%d dBm)", Net.ip().toString().c_str(), Net.rssi());
  else snprintf(v, sizeof v, "%s", Net.stateName());
  row("Wi-Fi", v);
  if (Battery.level() >= 0)
    snprintf(v, sizeof v, "%d%%%s, %d mV", Battery.level(), Battery.charging() ? " charging" : "", Battery.millivolts());
  else
    snprintf(v, sizeof v, "unknown");
  row("Battery", v);
  snprintf(v, sizeof v, "%u KB heap, %u KB PSRAM", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getFreePsram() / 1024));
  row("Free", v);
  const uint32_t up = millis() / 1000;
  snprintf(v, sizeof v, "%lud %02luh %02lum", (unsigned long)(up / 86400), (unsigned long)(up / 3600 % 24),
           (unsigned long)(up / 60 % 60));
  row("Uptime", v);
  row("Wi-Fi MAC", WiFi.macAddress().c_str());
  draw::hintBar(c, nullptr, "Back", nullptr);
}

// ===========================================================================
// Confirm
// ===========================================================================
void ConfirmScreen::ask(const char* title, const char* message, std::function<void()> onYes) {
  strlcpy(title_, title, sizeof title_);
  strlcpy(msg_, message, sizeof msg_);
  yes_ = onYes;
  UI.go(ScreenId::Confirm);
}

void ConfirmScreen::onEvent(const InputEvent& e) {
  if (e.type != Press::Short) return;
  if (e.btn == Btn::A) {
    UI.click();
    UI.back();
  } else if (e.btn == Btn::C) {
    UI.click();
    UI.back();
    if (yes_) yes_();
  }
}

void ConfirmScreen::render(Canvas& c, uint32_t now) {
  c.fillScreen(CREAM);
  draw::card(c, 20, 26, 280, 168, 20, SNOW, SHADOW, 4);
  chars::draw(c, chars::Kind::Bunny, 60, 84, 22, now, chars::Mood::Happy, SNOW);
  draw::text(c, title_, 100, 66, draw::FONT_TITLE, RED_DEEP);
  // Message: split on '\n' into up to 3 lines.
  char buf[80];
  strlcpy(buf, msg_, sizeof buf);
  int y = 120;
  char* save = nullptr;
  for (char* line = strtok_r(buf, "\n", &save); line && y < 190; line = strtok_r(nullptr, "\n", &save)) {
    draw::textC(c, line, 160, y, draw::FONT_BODY, INK);
    y += 22;
  }
  draw::hintBar(c, "No", nullptr, "Yes");
}
