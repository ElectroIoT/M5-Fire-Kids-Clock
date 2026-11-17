// Sound catalogue shared by the alarm editor, settings and the audio engine.
#pragma once
#include <stdint.h>

namespace sounds {

// Alarm sounds (selectable per alarm). Built-in melodies first, then optional
// WAV files on the microSD card.
enum AlarmSound : uint8_t {
  GENTLE_WAKE = 0,   // soft rising melody
  CARTOON_ALARM,     // boingy cartoon alarm
  MORNING_SONG,      // "Are you sleeping" (Frere Jacques, traditional)
  TWINKLE,           // Twinkle Twinkle Little Star (traditional)
  BIRDS,             // chirpy bird calls
  SD_ALARM1,         // /sounds/alarm1.wav
  SD_ALARM2,         // /sounds/alarm2.wav
  SD_MORNING,        // /sounds/morning.wav
  ALARM_SOUND_COUNT
};

// Effects used by the UI / routine (not selectable for alarms).
enum Effect : uint8_t {
  FX_CLICK = 0,
  FX_TASK_DONE,
  FX_REWARD,
  FX_NOTIFY,
  FX_MORNING,
  FX_BEDTIME,        // Brahms' Lullaby (public domain)
  FX_COUNT
};

inline const char* alarmSoundName(uint8_t s) {
  static const char* const n[ALARM_SOUND_COUNT] = {"Gentle wake", "Cartoon alarm", "Morning song", "Twinkle star",
                                                   "Birdies",     "SD: alarm1",    "SD: alarm2",   "SD: morning"};
  return s < ALARM_SOUND_COUNT ? n[s] : "?";
}

}  // namespace sounds
