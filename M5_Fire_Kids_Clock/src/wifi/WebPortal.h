// Parent setup web page, served only while "Parent Setup" is open on the clock
// (its own Wi-Fi access point with a one-time password, plus the home network
// IP if connected). Lets a parent type things that are painful with 3 buttons:
// Wi-Fi password, weather location, alarm labels, routine task names.
#pragma once
#include <Arduino.h>

class WebPortal {
 public:
  void begin();
  void end();
  void loop();
  bool running() const { return running_; }
  // Set when the parent saved something - the UI shows a toast.
  bool takeSavedFlag() { bool f = saved_; saved_ = false; return f; }
  void markSaved() { saved_ = true; }

 private:
  bool running_ = false;
  bool saved_ = false;
};

extern WebPortal Portal;
