#ifndef EEPROM_MOCK_H
#define EEPROM_MOCK_H

#include <stddef.h>
#include <stdint.h>
#include <map>

class EEPROMClass {
public:
  bool begin(size_t size) { return true; }
  void write(int address, uint8_t value) { _data[address] = value; }
  uint8_t read(int address) { return _data[address]; }
  bool commit() { return true; }

  std::map<int, uint8_t> _data;
};

extern EEPROMClass EEPROM;

#endif
