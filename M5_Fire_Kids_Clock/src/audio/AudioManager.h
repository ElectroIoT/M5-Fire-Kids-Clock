// Audio on the Fire's 1 W speaker (GPIO25 internal DAC, driven by M5Unified's
// Speaker class through I2S). Non-blocking: melodies are fed note by note into
// the speaker's two-slot queue from loop(), so the UI keeps running.
//
//   channel 0: alarms, previews, lullaby (can loop)
//   channel 1: short UI effects (click, task done, reward ...)
//
// microSD WAV files (8/16-bit PCM) are loaded into PSRAM and played with
// M5.Speaker.playWav(). MP3 is not decoded (see docs/AUDIO.md).
#pragma once
#include <Arduino.h>
#include "SoundList.h"
#include "Melodies.h"

class AudioManager {
 public:
  void begin();
  void loop();

  void startAlarm(uint8_t sound);   // loops until stop(); fades in if enabled
  void preview(uint8_t sound);      // one pass of an alarm sound at normal volume
  void effect(sounds::Effect fx);   // one-shot UI / reward sound
  void click();
  void stop();                      // stop everything immediately
  bool alarmPlaying() const { return mode_ != Mode::Idle; }
  void applyVolume();               // after the settings change

  bool sdReady() const { return sdOk_; }
  bool sdHas(uint8_t sound) const;

 private:
  enum class Mode : uint8_t { Idle, Melody, Wav };
  bool startWav(const char* path, bool loop);
  void freeWav();
  void playMelody(const melodies::Note* notes, uint16_t count, bool loop, int channel);
  void feed();

  Mode mode_ = Mode::Idle;
  const melodies::Note* notes_ = nullptr;
  uint16_t count_ = 0, pos_ = 0;
  bool loop_ = false;
  bool alarm_ = false;
  uint32_t alarmStartMs_ = 0;
  uint8_t* wav_ = nullptr;
  size_t wavLen_ = 0;
  bool wavLoop_ = false;
  bool sdOk_ = false;
  // Effect sequencer on channel 1
  const melodies::Note* fx_ = nullptr;
  uint16_t fxCount_ = 0, fxPos_ = 0;
};

extern AudioManager Audio;
