// Base class for every UI screen.
#pragma once
#include "../display/Draw.h"
#include "../input/InputManager.h"

class Screen {
 public:
  virtual ~Screen() = default;
  virtual void onEnter() {}
  virtual void onExit() {}
  virtual void onEvent(const InputEvent& e) = 0;
  virtual void render(Canvas& c, uint32_t now) = 0;
  // >0: animated, redraw every frameMs. 0: redraw only on change / new second.
  virtual uint32_t frameMs() const { return 0; }
  // Return to the home screen after a period without button presses?
  virtual bool timesOut() const { return true; }
  // Redraw once per second even when static (clock screens).
  virtual bool showsSeconds() const { return false; }
  virtual const char* name() const = 0;
};
