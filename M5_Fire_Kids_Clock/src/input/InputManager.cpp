#include "InputManager.h"
#include <M5Unified.h>

InputManager Input;

static m5::Button_Class& btnRef(int i) {
  return i == 0 ? M5.BtnA : (i == 1 ? M5.BtnB : M5.BtnC);
}

void InputManager::begin() {
  for (int i = 0; i < 3; ++i) {
    btnRef(i).setDebounceThresh(10);
    btnRef(i).setHoldThresh(kLongMs);
  }
  lastActivity_ = millis();
}

void InputManager::push(Btn b, Press p) {
  const uint8_t n = (head_ + 1) % 16;
  if (n == tail_) return;  // full: drop
  q_[head_] = {b, p};
  head_ = n;
  lastActivity_ = millis();
}

void InputManager::update() {
  const uint32_t ms = millis();
  for (int i = 0; i < 3; ++i) {
    auto& b = btnRef(i);
    const Btn id = static_cast<Btn>(i);
    if (b.wasPressed()) {
      longSent_[i] = false;
      lastActivity_ = ms;
    }
    if (b.wasHold()) {
      longSent_[i] = true;
      repeatAt_[i] = ms + kRepeatMs * 2;
      push(id, Press::Long);
    } else if (b.isPressed() && longSent_[i] && (int32_t)(ms - repeatAt_[i]) >= 0) {
      repeatAt_[i] = ms + kRepeatMs;
      push(id, Press::Repeat);
    }
    if (b.wasReleased() && !longSent_[i]) push(id, Press::Short);
    if (b.wasReleased()) longSent_[i] = false;
  }
}

bool InputManager::next(InputEvent& ev) {
  if (tail_ == head_) return false;
  ev = q_[tail_];
  tail_ = (tail_ + 1) % 16;
  return true;
}

void InputManager::inject(Btn b, Press p) {
  injectDownUntil_[static_cast<int>(b)] = millis() + 150;
  push(b, p);
}

bool InputManager::isDown(Btn b) const {
  const int i = static_cast<int>(b);
  return btnRef(i).isPressed() || (int32_t)(injectDownUntil_[i] - millis()) > 0;
}
