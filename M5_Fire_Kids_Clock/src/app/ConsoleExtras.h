// Console commands contributed by the alarm / audio / LED / routine / weather modules.
#pragma once

namespace console {
namespace extras {
void help();
void status();
bool exec(const char* cmd, char* args);  // true if handled
}  // namespace extras
}  // namespace console
