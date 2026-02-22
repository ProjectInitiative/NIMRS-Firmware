#ifndef EEPROM_MOCK_H
#define EEPROM_MOCK_H

#include <map>
#include <stddef.h>
#include <stdint.h>

class EEPROMClass {
public:
  std::map<int, uint8_t> _data;
  size_t _size = 512;

  bool begin(size_t size) {
    _size = size;
    return true;
  }

  void write(int address, uint8_t value) {
    if (address >= 0 && address < (int)_size) {
      _data[address] = value;
    }
  }

  uint8_t read(int address) {
    if (_data.find(address) != _data.end()) {
      return _data[address];
    }
    return 0xFF; // Default EEPROM value
  }

  bool commit() { return true; }

  uint8_t operator[](int address) { return read(address); }

  // Helper for tests
  void reset() { _data.clear(); }
};

extern EEPROMClass EEPROM;

#endif