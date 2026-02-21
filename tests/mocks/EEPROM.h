#ifndef EEPROM_MOCK_H
#define EEPROM_MOCK_H

#include <map>
#include <stddef.h>
#include <stdint.h>

class EEPROMClass {
public:
  bool begin(size_t size) { return true; }
  void write(int address, uint8_t value) { storage[address] = value; }
  uint8_t read(int address) {
    return storage.count(address) ? storage[address] : 0;
  }
  bool commit() { return true; }

  template <typename T> T &get(int address, T &t) { return t; }

  template <typename T> const T &put(int address, const T &t) { return t; }

  std::map<int, uint8_t> storage;
};

extern EEPROMClass EEPROM;

#endif
