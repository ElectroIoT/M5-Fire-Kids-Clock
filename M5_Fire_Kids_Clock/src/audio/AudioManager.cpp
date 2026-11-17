#include "AudioManager.h"
#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>
#include "../config/Config.h"
#include "../config/Pins.h"
#include "../settings/SettingsManager.h"

AudioManager Audio;

using melodies::Note;

// Soft, rounded waveform (one period, unsigned 8-bit) - gentler than a square wave.
static const uint8_t kSoftWave[16] = {128, 176, 218, 245, 255, 245, 218, 176, 128, 80, 38, 11, 1, 11, 38, 80};
static const uint8_t kSilence[64] = {128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
                                     128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
                                     128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
                                     128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128};
static constexpr int CH_MAIN = 0;
static constexpr int CH_FX = 1;
static constexpr size_t kMaxWavBytes = 3 * 1024 * 1024;

static const char* sdPath(uint8_t s) {
  switch (s) {
    case sounds::SD_ALARM1: return "/sounds/alarm1.wav";
    case sounds::SD_ALARM2: return "/sounds/alarm2.wav";
    case sounds::SD_MORNING: return "/sounds/morning.wav";
    default: return nullptr;
  }
}

static uint8_t masterFor(uint8_t level) {
  static const uint8_t curve[11] = {0, 40, 64, 88, 112, 136, 160, 184, 208, 232, 255};
  return curve[level > 10 ? 10 : level];
}

// Queue one tone (or a rest) on a channel; returns false if the queue is full.
static bool queueNote(int ch, uint16_t hz, uint32_t ms) {
  if (hz == 0) {
    const uint32_t reps = ms / 8 ? ms / 8 : 1;  // 64 samples @ 8 kHz = 8 ms
    return M5.Speaker.playRaw(kSilence, sizeof kSilence, 8000, false, reps, ch, false);
  }
  return M5.Speaker.tone(hz, ms, ch, false, kSoftWave, sizeof kSoftWave, false);
}

void AudioManager::begin() {
  M5.Speaker.begin();
  applyVolume();
  M5.Speaker.setChannelVolume(CH_FX, 200);
  sdOk_ = SD.begin(pins::SD_CS, SPI, 20000000);
  if (sdOk_) {
    KC_LOG("Audio: microSD ready (%llu MB), alarm1=%d alarm2=%d morning=%d", SD.cardSize() / (1024 * 1024),
           sdHas(sounds::SD_ALARM1), sdHas(sounds::SD_ALARM2), sdHas(sounds::SD_MORNING));
  } else {
    KC_LOG("Audio: no microSD card - built-in melodies only");
  }
}

void AudioManager::applyVolume() {
  const Settings& s = AppCfg.get();
  uint8_t level = s.volume;
  // Alarms must always be audible: never quieter than level 4 while ringing.
  if (alarm_ && level < 4) level = 4;
  M5.Speaker.setVolume(masterFor(level));
}

bool AudioManager::sdHas(uint8_t sound) const {
  const char* p = sdPath(sound);
  return sdOk_ && p && SD.exists(p);
}

void AudioManager::freeWav() {
  if (wav_) {
    M5.Speaker.stop(CH_MAIN);
    free(wav_);
    wav_ = nullptr;
    wavLen_ = 0;
  }
}

bool AudioManager::startWav(const char* path, bool loop) {
  if (!sdOk_ || !path) return false;
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  const size_t len = f.size();
  if (len < 44 || len > kMaxWavBytes) {
    KC_LOG("Audio: %s has unsupported size %u", path, (unsigned)len);
    f.close();
    return false;
  }
  freeWav();
  wav_ = static_cast<uint8_t*>(ps_malloc(len));
  if (!wav_) { f.close(); return false; }
  wavLen_ = f.read(wav_, len);
  f.close();
  if (wavLen_ != len || memcmp(wav_, "RIFF", 4) != 0 || memcmp(wav_ + 8, "WAVE", 4) != 0) {
    KC_LOG("Audio: %s is not a PCM WAV file", path);
    freeWav();
    return false;
  }
  if (!M5.Speaker.playWav(wav_, wavLen_, 1, CH_MAIN, true)) {
    freeWav();
    return false;
  }
  mode_ = Mode::Wav;
  wavLoop_ = loop;
  return true;
}

void AudioManager::playMelody(const Note* notes, uint16_t count, bool loop, int channel) {
  if (channel == CH_MAIN) {
    M5.Speaker.stop(CH_MAIN);
    freeWav();
    notes_ = notes;
    count_ = count;
    pos_ = 0;
    loop_ = loop;
    mode_ = Mode::Melody;
  } else {
    M5.Speaker.stop(CH_FX);
    fx_ = notes;
    fxCount_ = count;
    fxPos_ = 0;
  }
  feed();
}

// Each note is queued as "tone (88 %) + short gap (12 %)" so repeated notes are
// heard separately. pos counts half-steps: even = tone, odd = gap.
void AudioManager::feed() {
  if (mode_ == Mode::Melody && notes_) {
    while (M5.Speaker.isPlaying(CH_MAIN) < 2) {
      if (pos_ >= count_ * 2) {
        if (!loop_) { mode_ = Mode::Idle; notes_ = nullptr; break; }
        pos_ = 0;
      }
      const Note& n = notes_[pos_ / 2];
      const bool gap = pos_ & 1;
      const uint32_t ms = gap ? (n.hz ? n.ms * 12 / 100 : 0) : (n.hz ? n.ms * 88 / 100 : n.ms);
      if (ms && !queueNote(CH_MAIN, gap ? 0 : n.hz, ms)) break;
      ++pos_;
    }
  }
  if (fx_) {
    while (M5.Speaker.isPlaying(CH_FX) < 2) {
      if (fxPos_ >= fxCount_ * 2) { fx_ = nullptr; break; }
      const Note& n = fx_[fxPos_ / 2];
      const bool gap = fxPos_ & 1;
      const uint32_t ms = gap ? (n.hz ? n.ms / 8 : 0) : n.ms;
      if (ms && !queueNote(CH_FX, gap ? 0 : n.hz, ms)) break;
      ++fxPos_;
    }
  }
}

void AudioManager::loop() {
  feed();
  if (mode_ == Mode::Wav && !M5.Speaker.isPlaying(CH_MAIN)) {
    if (wavLoop_) M5.Speaker.playWav(wav_, wavLen_, 1, CH_MAIN, true);
    else { freeWav(); mode_ = Mode::Idle; }
  }
  // Gentle fade-in for alarms: 25 % -> 100 % over KC_ALARM_RAMP_MS.
  if (alarm_) {
    uint8_t v = 255;
    if (AppCfg.get().gradualVolume) {
      const uint32_t t = millis() - alarmStartMs_;
      v = t >= KC_ALARM_RAMP_MS ? 255 : (uint8_t)(64 + (191UL * t) / KC_ALARM_RAMP_MS);
    }
    M5.Speaker.setChannelVolume(CH_MAIN, v);
  }
}

void AudioManager::startAlarm(uint8_t sound) {
  stop();
  alarm_ = true;
  alarmStartMs_ = millis();
  applyVolume();
  M5.Speaker.setChannelVolume(CH_MAIN, AppCfg.get().gradualVolume ? 64 : 255);
  if (sdPath(sound) && startWav(sdPath(sound), true)) return;
  switch (sound) {
    case sounds::GENTLE_WAKE: playMelody(KC_MELODY(melodies::GENTLE_WAKE), true, CH_MAIN); break;
    case sounds::MORNING_SONG: playMelody(KC_MELODY(melodies::MORNING_SONG), true, CH_MAIN); break;
    case sounds::TWINKLE: playMelody(KC_MELODY(melodies::TWINKLE), true, CH_MAIN); break;
    case sounds::BIRDS: playMelody(KC_MELODY(melodies::BIRDS), true, CH_MAIN); break;
    default: playMelody(KC_MELODY(melodies::CARTOON_ALARM), true, CH_MAIN); break;  // also SD fallback
  }
}

void AudioManager::preview(uint8_t sound) {
  stop();
  M5.Speaker.setChannelVolume(CH_MAIN, 200);
  if (sdPath(sound)) {
    if (!startWav(sdPath(sound), false)) effect(sounds::FX_NOTIFY);  // file missing: short hint
    return;
  }
  switch (sound) {
    case sounds::GENTLE_WAKE: playMelody(KC_MELODY(melodies::GENTLE_WAKE), false, CH_MAIN); break;
    case sounds::MORNING_SONG: playMelody(KC_MELODY(melodies::MORNING_SONG), false, CH_MAIN); break;
    case sounds::TWINKLE: playMelody(KC_MELODY(melodies::TWINKLE), false, CH_MAIN); break;
    case sounds::BIRDS: playMelody(KC_MELODY(melodies::BIRDS), false, CH_MAIN); break;
    default: playMelody(KC_MELODY(melodies::CARTOON_ALARM), false, CH_MAIN); break;
  }
}

void AudioManager::effect(sounds::Effect fx) {
  const Settings& s = AppCfg.get();
  if (s.muted) return;  // quiet mode silences effects; alarms are never muted
  switch (fx) {
    case sounds::FX_CLICK: playMelody(KC_MELODY(melodies::FX_CLICK), false, CH_FX); break;
    case sounds::FX_TASK_DONE: playMelody(KC_MELODY(melodies::FX_TASK_DONE), false, CH_FX); break;
    case sounds::FX_REWARD: playMelody(KC_MELODY(melodies::FX_REWARD), false, CH_FX); break;
    case sounds::FX_NOTIFY: playMelody(KC_MELODY(melodies::FX_NOTIFY), false, CH_FX); break;
    case sounds::FX_MORNING:
      if (!startWav("/sounds/morning.wav", false)) playMelody(KC_MELODY(melodies::FX_MORNING), false, CH_FX);
      break;
    case sounds::FX_BEDTIME:
      M5.Speaker.setChannelVolume(CH_MAIN, 120);  // soft lullaby
      if (!startWav("/sounds/bedtime.wav", false)) playMelody(KC_MELODY(melodies::FX_BEDTIME), false, CH_MAIN);
      break;
    default: break;
  }
}

void AudioManager::click() {
  const Settings& s = AppCfg.get();
  if (s.clickSounds && !s.muted && !alarm_) effect(sounds::FX_CLICK);
}

void AudioManager::stop() {
  M5.Speaker.stop();  // all channels, immediately
  freeWav();
  notes_ = nullptr;
  fx_ = nullptr;
  mode_ = Mode::Idle;
  alarm_ = false;
  applyVolume();
}
