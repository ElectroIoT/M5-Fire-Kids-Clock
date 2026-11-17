// Phase A spike (Android app plan): how much RAM does Bluetooth LE cost on the
// Fire next to Wi-Fi + web server + UI, and do Wi-Fi and BLE coexist?
//
//   KC_BLE_SPIKE 0 = not built (normal firmware)
//   KC_BLE_SPIKE 1 = Bluedroid (Arduino BLE library, core's own stack)
//   KC_BLE_SPIKE 2 = NimBLE-Arduino
//
// Console: "ble on" starts a GATT server + advertising, "ble off", "ble mem".
#pragma once
#include <Arduino.h>

#ifndef KC_BLE_SPIKE
  #define KC_BLE_SPIKE 0
#endif

namespace blespike {
const char* stackName();
bool start();
void stop();
bool running();
void printMem(Print& out, const char* tag);
}  // namespace blespike
