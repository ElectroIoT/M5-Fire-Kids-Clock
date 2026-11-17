// Top-level wiring: initialises every module and runs the cooperative,
// non-blocking main loop.
#pragma once
#include <stdint.h>

namespace app {
void setup();
void loop();
const char* sectionName(int i);
uint32_t sectionWorst(int i);  // us, last 10 s window
}  // namespace app
