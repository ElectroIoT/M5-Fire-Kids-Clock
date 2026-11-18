// Local JSON API for the companion app: http://kidsclock.local:8080/api/v1/...
// Runs whenever the clock is on the home Wi-Fi (station mode). Every endpoint
// except /info and /pair needs "Authorization: Bearer <token>" (see Pairing).
// Announced over mDNS as _kidsclock._tcp. See docs/API.md.
#pragma once
#include <Arduino.h>

namespace api {
constexpr uint16_t kPort = 8080;
void begin();
void loop();
uint32_t requests();
}  // namespace api
