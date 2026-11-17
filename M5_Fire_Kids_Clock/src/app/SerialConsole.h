// USB serial debug console (115200 baud). Type "help" for commands.
// Lets you inspect state, simulate button presses, set the clock to edge cases
// (e.g. 11:59 PM) and grab screenshots - useful for testing without touching
// the device.
#pragma once
#include <Arduino.h>

namespace console {
void begin();
void loop();
}  // namespace console
