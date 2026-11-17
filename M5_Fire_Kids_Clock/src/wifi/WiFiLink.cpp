#include "WiFiLink.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <esp_random.h>
#include "../config/Config.h"
#include "../settings/SettingsManager.h"
#include "../clock/ClockManager.h"
#include "WebPortal.h"

WiFiLink Net;
static DNSServer dns;

bool WiFiLink::wanted() const {
  return AppCfg.get().wifiEnabled && !suspended_ && (AppCfg.wifi().ssid[0] != '\0' || AppCfg.wifi().ssid2[0] != '\0');
}

void WiFiLink::begin() {
  WiFi.persistent(false);         // credentials live in our own NVS record
  WiFi.setAutoReconnect(false);   // reconnect is handled here with back-off
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(KC_HOSTNAME);
  WiFi.setSleep(true);            // modem sleep: lower power, plenty for NTP/weather
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(apSsid_, sizeof apSsid_, KC_AP_PREFIX "%02X%02X", mac[4], mac[5]);
  applyEnabled();
}

void WiFiLink::applyEnabled() {
  if (wanted()) {
    if (state_ == State::Disabled || state_ == State::NoNetwork) startConnect();
  } else {
    goDown(true);
    state_ = (AppCfg.get().wifiEnabled && !suspended_) ? State::NoNetwork : State::Disabled;
    stateMs_ = millis();
  }
}

void WiFiLink::setSuspended(bool s) {
  if (s == suspended_) return;
  suspended_ = s;
  KC_LOG("Wi-Fi: %s for night mode", s ? "paused" : "resumed");
  if (portal_) return;  // never cut the parent setup page
  applyEnabled();
}

void WiFiLink::reconnectNow() {
  goDown(true);
  state_ = State::NoNetwork;
  retryDelayMs_ = 0;
  network_ = 0;
  if (wanted()) startConnect();
}

void WiFiLink::startConnect() {
  const WifiCreds& c = AppCfg.wifi();
  if (network_ == 1 && !c.ssid2[0]) network_ = 0;
  if (network_ == 0 && !c.ssid[0] && c.ssid2[0]) network_ = 1;
  const char* s = network_ ? c.ssid2 : c.ssid;
  KC_LOG("Wi-Fi: connecting to '%s'%s", s, network_ ? " (backup)" : "");
  WiFi.begin(s, network_ ? c.pass2 : c.pass);
  state_ = State::Connecting;
  stateMs_ = millis();
}

void WiFiLink::goDown(bool notify) {
  const bool was = state_ == State::Connected;
  WiFi.disconnect(false);  // station only; a running setup AP stays up
  if (was && notify) Clock.onNetworkDown();
}

void WiFiLink::loop() {
  // Web requests are handled first: they may restart timers (touchPortal,
  // reconnectNow). Reading the time only afterwards keeps every "now - start"
  // below from going negative and wrapping around to a huge value.
  if (portal_) {
    dns.processNextRequest();
    Portal.loop();
    if (millis() - portalTouchMs_ > KC_PORTAL_IDLE_TIMEOUT_MS) {
      KC_LOG("Wi-Fi: setup page idle, closing access point");
      stopPortal();
    }
  }
  const uint32_t ms = millis();
  const wl_status_t st = WiFi.status();

  switch (state_) {
    case State::Disabled:
    case State::NoNetwork:
      break;

    case State::Connecting:
      if (st == WL_CONNECTED) {
        state_ = State::Connected;
        stateMs_ = ms;
        retryDelayMs_ = 0;
        KC_LOG("Wi-Fi: connected, IP %s, RSSI %d dBm", WiFi.localIP().toString().c_str(), WiFi.RSSI());
        Clock.onNetworkUp();
      } else if (ms - stateMs_ > KC_WIFI_CONNECT_TIMEOUT_MS ||
                 // WiFi.status() still shows the previous attempt for a moment
                 // after WiFi.begin(): only trust failure codes after 4 s.
                 (ms - stateMs_ > 4000 && (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL))) {
        WiFi.disconnect(false);
        if (AppCfg.wifi().ssid2[0] && network_ == 0) {
          network_ = 1;  // main network failed: try the backup straight away
          startConnect();
          break;
        }
        network_ = 0;  // next round starts with the main network again
        retryDelayMs_ = retryDelayMs_ ? min<uint32_t>(retryDelayMs_ * 2, KC_WIFI_RETRY_MAX_MS) : KC_WIFI_RETRY_MIN_MS;
        state_ = State::WaitRetry;
        stateMs_ = ms;
        KC_LOG("Wi-Fi: connect failed (status %d), retry in %lus", (int)st, (unsigned long)(retryDelayMs_ / 1000));
      }
      break;

    case State::Connected:
      if (st != WL_CONNECTED) {
        KC_LOG("Wi-Fi: connection lost - clock keeps running offline");
        Clock.onNetworkDown();
        WiFi.disconnect(false);
        retryDelayMs_ = KC_WIFI_RETRY_MIN_MS / 3;  // first retry quickly
        state_ = State::WaitRetry;
        stateMs_ = ms;
      }
      break;

    case State::WaitRetry:
      // While the parent page is open, scanning for a dead network would make
      // the access point hop channels; wait for new credentials instead.
      if (!portal_ && ms - stateMs_ >= retryDelayMs_) {
        if (wanted()) startConnect();
        else applyEnabled();
      }
      break;
  }
}

int WiFiLink::rssi() const { return connected() ? WiFi.RSSI() : -127; }

int WiFiLink::bars() const {
  if (!connected()) return 0;
  const int r = WiFi.RSSI();
  return r > -60 ? 3 : r > -72 ? 2 : 1;
}

const char* WiFiLink::ssid() const { return network_ ? AppCfg.wifi().ssid2 : AppCfg.wifi().ssid; }

IPAddress WiFiLink::ip() const { return connected() ? WiFi.localIP() : IPAddress(0, 0, 0, 0); }
IPAddress WiFiLink::apIp() const { return portal_ ? WiFi.softAPIP() : IPAddress(0, 0, 0, 0); }

const char* WiFiLink::stateName() const {
  switch (state_) {
    case State::Disabled: return suspended_ ? "paused (night)" : "off";
    case State::NoNetwork: return "no network saved";
    case State::Connecting: return "connecting";
    case State::Connected: return "connected";
    default: return "retrying";
  }
}

bool WiFiLink::startPortal() {
  if (portal_) { touchPortal(); return true; }
  // Fresh 8-digit password each time the page is opened.
  snprintf(apPass_, sizeof apPass_, "%08lu", (unsigned long)(esp_random() % 100000000UL));
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);  // modem sleep makes the setup pages time out in AP+STA mode
  if (!WiFi.softAP(apSsid_, apPass_)) {
    KC_LOG("Wi-Fi: softAP failed");
    WiFi.mode(WIFI_STA);
    return false;
  }
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());
  Portal.begin();
  WiFi.scanNetworks(true);  // async scan for the network list
  portal_ = true;
  touchPortal();
  KC_LOG("Wi-Fi: setup AP '%s' up at %s", apSsid_, WiFi.softAPIP().toString().c_str());
  return true;
}

void WiFiLink::stopPortal() {
  if (!portal_) return;
  Portal.end();
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);   // back to power saving
  portal_ = false;
  KC_LOG("Wi-Fi: setup AP closed");
  if (state_ == State::WaitRetry) stateMs_ = millis() - retryDelayMs_;  // retry right away
  if (!connected()) applyEnabled();
}
