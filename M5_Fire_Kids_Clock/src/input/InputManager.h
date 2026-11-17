// Front-button input: debounced short press, long press and auto-repeat,
// built on M5Unified's Button_Class. Serial commands can inject presses too,
// which makes the UI testable over USB.
#pragma once
#include <Arduino.h>

enum class Btn : uint8_t { A = 0, B = 1, C = 2 };
enum class Press : uint8_t { Short, Long, Repeat };

struct InputEvent {
  Btn btn;
  Press type;
};

class InputManager {
 public:
  void begin();
  void update();                       // call once per loop after M5.update()
  bool next(InputEvent& ev);           // pop one event
  void inject(Btn b, Press p);         // from the serial console
  bool isDown(Btn b) const;            // for on-screen press feedback
  uint32_t lastActivityMs() const { return lastActivity_; }
  void markActivity() { lastActivity_ = millis(); }
  void clear() { head_ = tail_ = 0; }

  static constexpr uint32_t kLongMs = 700;
  static constexpr uint32_t kRepeatMs = 140;

 private:
  void push(Btn b, Press p);
  InputEvent q_[16];
  uint8_t head_ = 0, tail_ = 0;
  uint32_t lastActivity_ = 0;
  uint32_t repeatAt_[3] = {0, 0, 0};
  bool longSent_[3] = {false, false, false};
  uint32_t injectDownUntil_[3] = {0, 0, 0};
};

extern InputManager Input;
