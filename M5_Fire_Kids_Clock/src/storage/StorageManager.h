// Versioned, CRC-protected blob storage on top of ESP32 Preferences (NVS).
//
// Every record is stored as [header | payload]. On load:
//   * missing / bad magic / bad CRC  -> caller keeps its defaults (returns false)
//   * older, shorter payload         -> copied over the defaults, so fields added
//                                       in a newer firmware keep their default
//                                       values (append-only migration)
// NVS is not erased by a normal firmware upload, so settings survive updates.
#pragma once
#include <Arduino.h>

class StorageManager {
 public:
  enum class LoadResult : uint8_t { Ok, Migrated, Missing, Corrupt };

  bool begin();
  LoadResult loadBlob(const char* key, uint16_t version, void* data, size_t size);
  bool saveBlob(const char* key, uint16_t version, const void* data, size_t size);

  uint32_t getU32(const char* key, uint32_t def);
  bool putU32(const char* key, uint32_t value);

  // Erases every key this firmware owns (factory reset).
  bool eraseAll();

  static const char* resultName(LoadResult r);

 private:
  struct Header {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t crc;
  };
  static constexpr uint32_t kMagic = 0x4B434C4BUL;  // "KCLK"
  static constexpr const char* kNamespace = "kidsclock";
  bool ready_ = false;
};

extern StorageManager Storage;
