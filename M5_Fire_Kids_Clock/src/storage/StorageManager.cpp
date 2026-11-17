#include "StorageManager.h"
#include <Preferences.h>
#include <esp_rom_crc.h>
#include "../config/Config.h"

StorageManager Storage;

static Preferences prefs;

static uint32_t crcOf(const void* data, size_t len) {
  return esp_rom_crc32_le(0, static_cast<const uint8_t*>(data), len);
}

bool StorageManager::begin() {
  ready_ = prefs.begin(kNamespace, false);
  if (!ready_) KC_LOG("NVS: open failed");
  return ready_;
}

StorageManager::LoadResult StorageManager::loadBlob(const char* key, uint16_t version, void* data, size_t size) {
  if (!ready_) return LoadResult::Missing;
  const size_t stored = prefs.getBytesLength(key);
  if (stored == 0) return LoadResult::Missing;
  if (stored < sizeof(Header) || stored > sizeof(Header) + 4096) return LoadResult::Corrupt;

  uint8_t* buf = static_cast<uint8_t*>(malloc(stored));
  if (!buf) return LoadResult::Corrupt;
  prefs.getBytes(key, buf, stored);

  Header h;
  memcpy(&h, buf, sizeof h);
  const size_t payload = stored - sizeof(Header);
  LoadResult res = LoadResult::Corrupt;
  if (h.magic == kMagic && h.size == payload && crcOf(buf + sizeof h, payload) == h.crc) {
    if (h.version == version && payload == size) {
      memcpy(data, buf + sizeof h, size);
      res = LoadResult::Ok;
    } else if (payload <= size) {
      // Older (shorter) layout: keep defaults for the new trailing fields.
      memcpy(data, buf + sizeof h, payload);
      res = LoadResult::Migrated;
    }
  }
  free(buf);
  return res;
}

bool StorageManager::saveBlob(const char* key, uint16_t version, const void* data, size_t size) {
  if (!ready_ || size > 4096) return false;
  const size_t total = sizeof(Header) + size;
  uint8_t* buf = static_cast<uint8_t*>(malloc(total));
  if (!buf) return false;
  Header h{kMagic, version, static_cast<uint16_t>(size), crcOf(data, size)};
  memcpy(buf, &h, sizeof h);
  memcpy(buf + sizeof h, data, size);
  const bool ok = prefs.putBytes(key, buf, total) == total;
  free(buf);
  if (!ok) KC_LOG("NVS: save '%s' failed", key);
  return ok;
}

uint32_t StorageManager::getU32(const char* key, uint32_t def) {
  return ready_ ? prefs.getULong(key, def) : def;
}

bool StorageManager::putU32(const char* key, uint32_t value) {
  return ready_ && prefs.putULong(key, value) == sizeof(uint32_t);
}

bool StorageManager::eraseAll() {
  return ready_ && prefs.clear();
}

const char* StorageManager::resultName(LoadResult r) {
  switch (r) {
    case LoadResult::Ok: return "ok";
    case LoadResult::Migrated: return "migrated";
    case LoadResult::Missing: return "missing";
    default: return "corrupt";
  }
}
