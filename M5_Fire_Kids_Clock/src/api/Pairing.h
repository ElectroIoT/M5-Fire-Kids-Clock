// Phone pairing for the local JSON API.
//
// The clock shows a 6-digit code for 2 minutes (Settings > Pair phone app).
// The app sends it once (POST /api/v1/pair) and gets a random 128-bit token that
// must accompany every later request ("Authorization: Bearer <32 hex chars>").
// Up to 4 phones; tokens live in NVS and can be revoked on the clock.
#pragma once
#include <Arduino.h>

class Pairing {
 public:
  static constexpr int kMaxPhones = 4;
  static constexpr uint32_t kCodeValidMs = 120000;

  void begin();
  void loop();

  // Pairing window
  void startWindow();                    // new random code, valid 2 min
  void cancelWindow();
  bool windowOpen() const { return codeValid_; }
  const char* code() const { return code_; }
  uint32_t secondsLeft() const;
  bool lockedOut() const { return (int32_t)(lockUntil_ - millis()) > 0; }

  // Returns true and fills tokenHex (33 bytes) when the code is right.
  enum class Result : uint8_t { Ok, NoWindow, WrongCode, LockedOut, Full };
  Result pair(const char* code, const char* phoneName, char* tokenHex);

  bool check(const char* tokenHex) const;  // constant-time; for the API
  int count() const;
  const char* phoneName(int i) const;
  void revokeAll();
  bool takeChanged() { bool c = changed_; changed_ = false; return c; }  // UI refresh

 private:
  struct Phone {
    uint8_t used;
    uint8_t reserved[3];
    uint8_t token[16];
    char name[20];
    uint32_t pairedEpoch;
  };
  struct Store {
    Phone phones[kMaxPhones];
  };
  void save();
  Store store_{};
  char code_[7] = {0};
  bool codeValid_ = false;
  uint32_t codeUntil_ = 0;
  uint8_t failures_ = 0;
  uint32_t lockUntil_ = 0;
  bool changed_ = false;
};

extern Pairing Pair;
