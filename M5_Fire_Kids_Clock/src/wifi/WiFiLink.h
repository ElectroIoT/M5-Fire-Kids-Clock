// Non-blocking Wi-Fi station manager with back-off reconnect and an on-demand
// parent setup access point. (Named WiFiLink to avoid clashing with the popular
// "WiFiManager" Arduino library.)
//
// Wi-Fi is optional: the clock, alarms and routines never wait for it.
#pragma once
#include <Arduino.h>
#include <IPAddress.h>

class WiFiLink {
 public:
  enum class State : uint8_t { Disabled, NoNetwork, Connecting, Connected, WaitRetry };

  void begin();
  void loop();

  State state() const { return state_; }
  bool connected() const { return state_ == State::Connected; }
  int rssi() const;
  int bars() const;               // 0..3 for the status icon
  IPAddress ip() const;
  const char* stateName() const;
  const char* ssid() const;       // network currently used / tried

  // Settings / night-mode control
  void applyEnabled();            // re-read AppCfg.wifiEnabled
  void setSuspended(bool s);      // night-mode "Wi-Fi quiet" (does not touch the setting)
  void reconnectNow();            // after new credentials

  // Parent setup access point
  bool startPortal();
  void stopPortal();
  bool portalActive() const { return portal_; }
  const char* apSsid() const { return apSsid_; }
  const char* apPass() const { return apPass_; }
  IPAddress apIp() const;
  void touchPortal() { portalTouchMs_ = millis(); }

 private:
  void startConnect();
  int network_ = 0;               // 0 = main, 1 = backup
  void goDown(bool notify);
  bool wanted() const;

  State state_ = State::Disabled;
  bool suspended_ = false;
  bool portal_ = false;
  uint32_t stateMs_ = 0;
  uint32_t retryDelayMs_ = 0;
  uint32_t portalTouchMs_ = 0;
  char apSsid_[24] = {0};
  char apPass_[12] = {0};
};

extern WiFiLink Net;
