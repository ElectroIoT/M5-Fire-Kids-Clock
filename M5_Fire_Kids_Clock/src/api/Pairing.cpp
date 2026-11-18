#include "Pairing.h"
#include <esp_random.h>
#include <time.h>
#include "../config/Config.h"
#include "../storage/StorageManager.h"

Pairing Pair;

static constexpr uint16_t kVersion = 1;

static void toHex(const uint8_t* b, size_t n, char* out) {
  static const char* h = "0123456789abcdef";
  for (size_t i = 0; i < n; ++i) {
    out[i * 2] = h[b[i] >> 4];
    out[i * 2 + 1] = h[b[i] & 15];
  }
  out[n * 2] = '\0';
}

static bool fromHex(const char* s, uint8_t* out, size_t n) {
  if (!s || strlen(s) != n * 2) return false;
  for (size_t i = 0; i < n; ++i) {
    uint8_t v = 0;
    for (int k = 0; k < 2; ++k) {
      const char c = s[i * 2 + k];
      v <<= 4;
      if (c >= '0' && c <= '9') v |= c - '0';
      else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
      else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
      else return false;
    }
    out[i] = v;
  }
  return true;
}

void Pairing::begin() {
  memset(&store_, 0, sizeof store_);
  auto r = Storage.loadBlob("phones", kVersion, &store_, sizeof store_);
  if (r != StorageManager::LoadResult::Ok && r != StorageManager::LoadResult::Migrated) memset(&store_, 0, sizeof store_);
  for (auto& p : store_.phones) p.name[sizeof p.name - 1] = '\0';
  KC_LOG("Pairing: %d phone(s) paired", count());
}

void Pairing::save() { Storage.saveBlob("phones", kVersion, &store_, sizeof store_); }

void Pairing::loop() {
  if (codeValid_ && (int32_t)(codeUntil_ - millis()) <= 0) {
    codeValid_ = false;
    changed_ = true;
    KC_LOG("Pairing: code expired");
  }
}

void Pairing::startWindow() {
  snprintf(code_, sizeof code_, "%06lu", (unsigned long)(esp_random() % 1000000UL));
  codeValid_ = true;
  codeUntil_ = millis() + kCodeValidMs;
  failures_ = 0;
  changed_ = true;
  KC_LOG("Pairing: window open for 2 min");  // the code itself is shown on screen only
}

void Pairing::cancelWindow() {
  codeValid_ = false;
  changed_ = true;
}

uint32_t Pairing::secondsLeft() const {
  if (!codeValid_) return 0;
  const int32_t ms = (int32_t)(codeUntil_ - millis());
  return ms > 0 ? (uint32_t)(ms + 999) / 1000 : 0;
}

Pairing::Result Pairing::pair(const char* code, const char* phoneName, char* tokenHex) {
  if (lockedOut()) return Result::LockedOut;
  if (!codeValid_) return Result::NoWindow;
  // Constant-time compare of the 6 digits.
  uint8_t diff = (code && strlen(code) == 6) ? 0 : 1;
  for (int i = 0; i < 6; ++i) diff |= (uint8_t)((code && code[i] ? code[i] : 0) ^ code_[i]);
  if (diff) {
    if (++failures_ >= 5) {  // brute-force guard: close the window and lock for 60 s
      codeValid_ = false;
      lockUntil_ = millis() + 60000;
      changed_ = true;
      KC_LOG("Pairing: too many wrong codes, locked for 60 s");
    }
    return Result::WrongCode;
  }
  int slot = -1;
  for (int i = 0; i < kMaxPhones; ++i)
    if (!store_.phones[i].used) { slot = i; break; }
  if (slot < 0) return Result::Full;
  Phone& p = store_.phones[slot];
  memset(&p, 0, sizeof p);
  p.used = 1;
  esp_fill_random(p.token, sizeof p.token);
  strlcpy(p.name, phoneName && *phoneName ? phoneName : "Phone", sizeof p.name);
  p.pairedEpoch = (uint32_t)time(nullptr);
  save();
  toHex(p.token, sizeof p.token, tokenHex);
  codeValid_ = false;
  changed_ = true;
  KC_LOG("Pairing: '%s' paired (slot %d)", p.name, slot + 1);
  return Result::Ok;
}

bool Pairing::check(const char* tokenHex) const {
  uint8_t t[16];
  if (!fromHex(tokenHex, t, sizeof t)) return false;
  bool ok = false;
  for (const auto& p : store_.phones) {
    uint8_t diff = p.used ? 0 : 1;
    for (int i = 0; i < 16; ++i) diff |= (uint8_t)(t[i] ^ p.token[i]);
    ok |= diff == 0;  // no early exit: constant time across slots
  }
  return ok;
}

int Pairing::count() const {
  int n = 0;
  for (const auto& p : store_.phones) n += p.used ? 1 : 0;
  return n;
}

const char* Pairing::phoneName(int i) const {
  int n = 0;
  for (const auto& p : store_.phones) {
    if (!p.used) continue;
    if (n++ == i) return p.name;
  }
  return "";
}

void Pairing::revokeAll() {
  memset(&store_, 0, sizeof store_);
  save();
  changed_ = true;
  KC_LOG("Pairing: all phones removed");
}
