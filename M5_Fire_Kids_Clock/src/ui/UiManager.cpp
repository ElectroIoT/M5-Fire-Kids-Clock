#include "UiManager.h"
#include "../config/Config.h"
#include "../display/DisplayManager.h"
#include "../display/Theme.h"
#include "../settings/SettingsManager.h"
#include "../clock/ClockManager.h"
#include "../app/NightMode.h"
#include "UiHooks.h"

UiManager UI;

void UiManager::begin() {
  current_ = ScreenId::Home;
  if (screens_[(int)current_]) screens_[(int)current_]->onEnter();
  dirty_ = true;
  applyBrightness();
}

void UiManager::go(ScreenId id, bool push) {
  Screen* next = screens_[(int)id];
  if (!next) return;
  if (push && id != current_ && depth_ < sizeof(stack_) / sizeof(stack_[0])) stack_[depth_++] = current_;
  if (Screen* cur = screens_[(int)current_]) cur->onExit();
  current_ = id;
  next->onEnter();
  Input.clear();
  dirty_ = true;
  KC_LOG("UI: -> %s", next->name());
}

void UiManager::back() {
  if (depth_ == 0) { home(); return; }
  go(stack_[--depth_], false);
}

void UiManager::home() {
  depth_ = 0;
  if (current_ != ScreenId::Home) go(ScreenId::Home, false);
}

void UiManager::toast(const char* msg, uint32_t ms) {
  strlcpy(toast_, msg, sizeof toast_);
  toastUntil_ = millis() + ms;
  dirty_ = true;
  KC_LOG("Toast: %s", msg);
}

void UiManager::click() { uihooks::click(); }

void UiManager::wake() {
  if (dimmed_) {
    dimmed_ = false;
    dirty_ = true;
  }
  Input.markActivity();
}

void UiManager::applyBrightness() {
  const Settings& s = AppCfg.get();
  uint8_t level = s.brightness;
  bool dim = false;
  const uint32_t idle = millis() - Input.lastActivityMs();
  if (current_ == ScreenId::Ring) {
    level = s.brightness < 8 ? 8 : s.brightness;   // alarms are always clearly visible
  } else if (NightMode.active()) {
    level = s.nightBrightness;
  } else if (s.idleDimMin && idle > s.idleDimMin * 60000UL) {
    dim = true;                                     // power saving
    level = 2;
  }
  if (dim != dimmed_) dirty_ = true;
  dimmed_ = dim;
  Gfx.setBrightness(level);
}

void UiManager::renderNow(uint32_t ms) {
  Screen* s = screens_[(int)current_];
  if (!s) return;
  Canvas& c = Gfx.canvas();
  const uint32_t r0 = micros();
  s->render(c, ms);
  renderUs_ = micros() - r0;
  if ((int32_t)(toastUntil_ - ms) > 0 && toast_[0]) {
    const bool night = NightMode.active() && current_ == ScreenId::Home;
    const int w = draw::textW(c, toast_, draw::FONT_BODY) + 36;
    const int x = (KC_SCREEN_W - w) / 2;
    draw::card(c, x, 46, w, 32, 16, night ? theme::NIGHT_BG2 : theme::SNOW, night ? 0 : theme::SHADOW);
    c.fillSmoothCircle(x + 16, 62, 6, night ? theme::NIGHT_DIM : theme::PINK_DEEP);
    draw::textC(c, toast_, x + w / 2 + 8, 63, draw::FONT_BODY, night ? theme::NIGHT_INK : theme::INK);
  }
  Gfx.push();
  lastFrameMs_ = ms;
  dirty_ = false;
}

void UiManager::loop() {
  const uint32_t ms = millis();
  Screen* s = screens_[(int)current_];
  if (!s) return;

  InputEvent ev;
  while (Input.next(ev)) {
    if (dimmed_) {  // first press only wakes the screen
      wake();
      continue;
    }
    wake();
    s->onEvent(ev);
    s = screens_[(int)current_];  // screen may have changed
    dirty_ = true;
  }

  // millis() (not `ms`): handling the events above may have updated the activity time.
  if (s->timesOut() && current_ != ScreenId::Home && millis() - Input.lastActivityMs() > KC_MENU_TIMEOUT_MS) {
    KC_LOG("UI: inactivity timeout");
    home();
    s = screens_[(int)current_];
  }

  if (toastUntil_ && (int32_t)(toastUntil_ - ms) <= 0) {
    toastUntil_ = 0;
    dirty_ = true;
  }

  applyBrightness();

  // Redraw policy: on change, on each new second for clock screens, or at the
  // screen's animation rate. Night / dimmed screens animate slowly.
  uint32_t frame = s->frameMs();
  if (dimmed_ && frame) frame = KC_FRAME_MS_NIGHT;
  const int sec = Clock.now().second;
  bool due = dirty_;
  if (!due && frame && ms - lastFrameMs_ >= frame) due = true;
  if (!due && s->showsSeconds() && sec != lastSecond_) due = true;
  if (due) {
    lastSecond_ = sec;
    renderNow(ms);
  }
}
