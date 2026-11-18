// Screen navigation, redraw scheduling, toasts and screen brightness.
#pragma once
#include <Arduino.h>
#include "Screen.h"

enum class ScreenId : uint8_t {
  Home,
  Settings,
  WifiSetup,
  SetTime,
  Info,
  Confirm,
  AlarmList,
  AlarmEdit,
  Ring,
  Routine,
  Stars,
  Reminder,
  Pair,
  Count
};

class UiManager {
 public:
  void begin();
  void loop();

  void registerScreen(ScreenId id, Screen* s) { screens_[(int)id] = s; }
  void go(ScreenId id, bool push = true);   // push: remember where we came from
  void back();
  void home();
  ScreenId current() const { return current_; }
  Screen* screen(ScreenId id) { return screens_[(int)id]; }

  void invalidate() { dirty_ = true; }
  void toast(const char* msg, uint32_t ms = 2500);
  void click();                    // button feedback sound

  // Brightness policy
  void wake();                     // any user activity
  bool dimmed() const { return dimmed_; }
  uint32_t lastRenderUs() const { return renderUs_; }

 private:
  void applyBrightness();
  void renderNow(uint32_t ms);

  Screen* screens_[(int)ScreenId::Count] = {nullptr};
  ScreenId current_ = ScreenId::Home;
  ScreenId stack_[6];
  uint8_t depth_ = 0;
  bool dirty_ = true;
  uint32_t lastFrameMs_ = 0;
  int lastSecond_ = -1;
  char toast_[48] = {0};
  uint32_t toastUntil_ = 0;
  bool dimmed_ = false;
  uint32_t renderUs_ = 0;
};

extern UiManager UI;
