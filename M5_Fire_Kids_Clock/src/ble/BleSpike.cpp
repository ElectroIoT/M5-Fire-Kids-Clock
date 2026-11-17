#include "BleSpike.h"
#include <esp_heap_caps.h>
#include <WiFi.h>
#include "../config/Config.h"

#if KC_BLE_SPIKE == 1
  #include <BLEDevice.h>
  #include <BLEServer.h>
  #include <BLE2902.h>
#elif KC_BLE_SPIKE == 2
  #include <NimBLEDevice.h>
#endif

namespace blespike {

// Example service layout the provisioning would use (UUIDs are random, spike only).
static constexpr const char* kSvc = "6b2f0001-8c1e-4d3a-9b55-1f0f6a1c0a01";
static constexpr const char* kStatus = "6b2f0002-8c1e-4d3a-9b55-1f0f6a1c0a01";
static constexpr const char* kWifi = "6b2f0003-8c1e-4d3a-9b55-1f0f6a1c0a01";
static bool s_running = false;

const char* stackName() {
#if KC_BLE_SPIKE == 1
  return "Bluedroid";
#elif KC_BLE_SPIKE == 2
  return "NimBLE";
#else
  return "none";
#endif
}

bool running() { return s_running; }

void printMem(Print& out, const char* tag) {
  out.printf("MEM %-10s stack=%s internal_free=%u largest_block=%u min_ever=%u psram_free=%u wifi=%d\n", tag,
             stackName(), (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM), WiFi.status() == WL_CONNECTED);
}

bool start() {
  if (s_running) return true;
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char name[24];
  snprintf(name, sizeof name, "KidsClock-%02X%02X", mac[4], mac[5]);
#if KC_BLE_SPIKE == 1
  BLEDevice::init(name);
  BLEServer* server = BLEDevice::createServer();
  BLEService* svc = server->createService(kSvc);
  BLECharacteristic* st = svc->createCharacteristic(kStatus, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  st->addDescriptor(new BLE2902());
  st->setValue("hello from the kids clock");
  svc->createCharacteristic(kWifi, BLECharacteristic::PROPERTY_WRITE);
  svc->start();
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(kSvc);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();
#elif KC_BLE_SPIKE == 2
  NimBLEDevice::init(name);
  NimBLEServer* server = NimBLEDevice::createServer();
  NimBLEService* svc = server->createService(kSvc);
  NimBLECharacteristic* st = svc->createCharacteristic(kStatus, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  st->setValue("hello from the kids clock");
  svc->createCharacteristic(kWifi, NIMBLE_PROPERTY::WRITE);
  svc->start();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(kSvc);
  adv->setScanResponse(true);
  adv->start();
#else
  (void)name;
  return false;
#endif
  s_running = true;
  KC_LOG("BLE spike: %s advertising as %s", stackName(), name);
  return true;
}

void stop() {
  if (!s_running) return;
#if KC_BLE_SPIKE == 1
  BLEDevice::deinit(false);  // keep controller memory (re-init possible)
#elif KC_BLE_SPIKE == 2
  NimBLEDevice::deinit(false);
#endif
  s_running = false;
  KC_LOG("BLE spike: stopped");
}

}  // namespace blespike
