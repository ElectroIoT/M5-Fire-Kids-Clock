// All UI screens of the M5 Fire Kids Clock.
#pragma once
#include <functional>
#include <vector>
#include "Screen.h"
#include "../config/Config.h"
#include "UiManager.h"

// ---------------------------------------------------------------------------
// Home (day) + night clock face
// ---------------------------------------------------------------------------
class HomeScreen : public Screen {
 public:
  void onEnter() override;
  void onEvent(const InputEvent& e) override;
  void render(Canvas& c, uint32_t now) override;
  uint32_t frameMs() const override;
  bool showsSeconds() const override { return true; }
  bool timesOut() const override { return false; }
  const char* name() const override { return "home"; }

 private:
  void renderDay(Canvas& c, uint32_t now);
  void renderDayStatic(Canvas& c, uint32_t now);
  void renderNight(Canvas& c, uint32_t now);
  void renderNotSet(Canvas& c, uint32_t now);
  void statusBar(Canvas& c, bool night);
  int dockIndex_ = 0;
  bool staticDirty_ = true;
};

// ---------------------------------------------------------------------------
// Generic scrolling menu (used for Settings)
// ---------------------------------------------------------------------------
struct MenuItem {
  enum Kind : uint8_t { Header, Action, Value, Toggle };
  const char* label;
  Kind kind;
  std::function<void(char*, size_t)> value;  // current value text
  std::function<void(int)> adjust;           // -1 / +1 (Value) ; toggle (Toggle)
  std::function<void()> action;              // Action
};

class MenuScreen : public Screen {
 public:
  explicit MenuScreen(const char* title, uint32_t color) : title_(title), color_(color) {}
  void onEnter() override;
  void onEvent(const InputEvent& e) override;
  void render(Canvas& c, uint32_t now) override;
  const char* name() const override { return title_; }

 protected:
  virtual void build(std::vector<MenuItem>& items) = 0;
  void rebuild();
  std::vector<MenuItem> items_;
  int sel_ = 1;
  int top_ = 0;
  bool editing_ = false;

 private:
  void move(int dir);
  const char* title_;
  uint32_t color_;
};

class SettingsScreen : public MenuScreen {
 public:
  SettingsScreen() : MenuScreen("Settings", 0xE3D6F7u) {}

 protected:
  void build(std::vector<MenuItem>& items) override;
};

// ---------------------------------------------------------------------------
// Parent setup (web page over a temporary access point)
// ---------------------------------------------------------------------------
class WifiSetupScreen : public Screen {
 public:
  void onEnter() override;
  void onEvent(const InputEvent& e) override;
  void render(Canvas& c, uint32_t now) override;
  uint32_t frameMs() const override { return 500; }
  bool timesOut() const override { return false; }
  const char* name() const override { return "parent-setup"; }
};

// ---------------------------------------------------------------------------
// Manual time & date (12-hour editor)
// ---------------------------------------------------------------------------
class SetTimeScreen : public Screen {
 public:
  void onEnter() override;
  void onEvent(const InputEvent& e) override;
  void render(Canvas& c, uint32_t now) override;
  const char* name() const override { return "set-time"; }

 private:
  void adjust(int dir);
  int field_ = 0;  // 0 day,1 month,2 year,3 hour,4 minute,5 ampm
  int day_ = 1, month_ = 1, year_ = 2026, hour12_ = 7, minute_ = 0;
  bool pm_ = false;
};

// ---------------------------------------------------------------------------
// Device information
// ---------------------------------------------------------------------------
class InfoScreen : public Screen {
 public:
  void onEvent(const InputEvent& e) override;
  void render(Canvas& c, uint32_t now) override;
  bool showsSeconds() const override { return true; }
  const char* name() const override { return "info"; }
};

// ---------------------------------------------------------------------------
// Yes / No confirmation
// ---------------------------------------------------------------------------
class ConfirmScreen : public Screen {
 public:
  void ask(const char* title, const char* message, std::function<void()> onYes);
  void onEvent(const InputEvent& e) override;
  void render(Canvas& c, uint32_t now) override;
  const char* name() const override { return "confirm"; }

 private:
  char title_[32] = {0};
  char msg_[80] = {0};
  std::function<void()> yes_;
};

extern ConfirmScreen ConfirmDlg;

// ---------------------------------------------------------------------------
// Alarms: list, editor (12-hour wizard) and the ringing screen
// ---------------------------------------------------------------------------
class AlarmListScreen : public Screen {
 public:
  void onEnter() override;
  void onEvent(const InputEvent& e) override;
  void render(Canvas& c, uint32_t now) override;
  const char* name() const override { return "alarms"; }

 private:
  int sel_ = 1;  // 0 = Back, 1..N = alarms
  int top_ = 0;
};

class AlarmEditScreen : public Screen {
 public:
  void edit(int index);
  void onEnter() override;
  void onEvent(const InputEvent& e) override;
  void render(Canvas& c, uint32_t now) override;
  const char* name() const override { return "alarm-edit"; }

 private:
  enum Field : uint8_t { F_HOUR, F_MIN, F_AMPM, F_REPEAT, F_DAYS, F_SOUND, F_LABEL, F_ON, F_SAVE, F_COUNT };
  void adjust(int dir);
  void nextField();
  void commit();
  int index_ = 0;
  uint8_t field_ = F_HOUR;
  int dayCursor_ = 1;  // Monday; 7 = "done"
  int hour12_ = 7, minute_ = 0;
  bool pm_ = false;
  uint8_t repeat_ = 0, days_ = 0, sound_ = 0, enabled_ = 1;
  char label_[24] = {0};
};

class RingScreen : public Screen {
 public:
  void onEvent(const InputEvent& e) override;
  void render(Canvas& c, uint32_t now) override;
  uint32_t frameMs() const override { return 80; }
  bool timesOut() const override { return false; }
  const char* name() const override { return "ringing"; }
};

extern AlarmEditScreen AlarmEditor;

// ---------------------------------------------------------------------------
// Daily routine checklist, stars / badges, and the reminder pop-up
// ---------------------------------------------------------------------------
class RoutineScreen : public Screen {
 public:
  void onEnter() override;
  void onEvent(const InputEvent& e) override;
  void render(Canvas& c, uint32_t now) override;
  uint32_t frameMs() const override { return celebrateUntil_ ? 70 : 0; }
  const char* name() const override { return "routine"; }

 private:
  void load();
  uint8_t period_ = 0;
  int rows_[KC_MAX_TASKS];
  int count_ = 0;
  int sel_ = 1;  // 0 = period chip, 1..count = tasks, count+1 = back
  int top_ = 0;
  uint32_t celebrateUntil_ = 0;
  bool bigCelebration_ = false;
};

class StarsScreen : public Screen {
 public:
  void onEvent(const InputEvent& e) override;
  void render(Canvas& c, uint32_t now) override;
  uint32_t frameMs() const override { return 90; }
  const char* name() const override { return "stars"; }
};

class ReminderScreen : public Screen {
 public:
  void show(int task);
  void onEvent(const InputEvent& e) override;
  void render(Canvas& c, uint32_t now) override;
  uint32_t frameMs() const override { return 90; }
  const char* name() const override { return "reminder"; }

 private:
  int task_ = -1;
};

extern ReminderScreen ReminderPopup;
