// Procedurally drawn cartoon friends. Each changes with the time of day:
//   morning   -> Sunny the smiling sun
//   afternoon -> Bun-Bun the cheerful bunny
//   evening   -> Miko the relaxed kitty
//   night     -> Luna the sleepy moon
#pragma once
#include "Draw.h"

namespace chars {

enum class Kind : uint8_t { Sun, Bunny, Kitty, Moon };
enum class Mood : uint8_t { Happy, Excited, Sleepy, Proud };

// cx, cy = centre; s = body radius in px (about 20..60). t = millis() for animation.
void draw(Canvas& c, Kind k, int cx, int cy, int s, uint32_t t, Mood mood, uint32_t bg);

Kind forHour(int hour24);
const char* name(Kind k);
const char* greeting(int hour24);
// "Good morning, <name>!" (or the plain greeting when no name is set)
void greetingFor(int hour24, const char* name, char* out, size_t n);

}  // namespace chars
