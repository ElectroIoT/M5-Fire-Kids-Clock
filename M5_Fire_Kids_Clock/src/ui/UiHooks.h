// Callbacks from the UI into other subsystems (implemented in app/App.cpp), so
// the UI layer stays independent of the audio / LED / alarm implementations.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace uihooks {

void click();                       // short button feedback sound
void previewSound(uint8_t alarmSound);  // alarm editor preview
void stopSound();
void celebrate(bool big);              // task done (small) / whole day done (big)
void reminderAlert();                  // sound + light for a routine reminder

// Home screen info. Each returns false when there is nothing to show.
bool nextAlarm(char* time12, size_t tn, char* label, size_t ln, char* when, size_t wn);
bool weather(uint8_t& kind, int& tempC, bool& day);
bool stars(int& today, int& total);
bool routineProgress(int& done, int& total, const char*& periodName);

}  // namespace uihooks
