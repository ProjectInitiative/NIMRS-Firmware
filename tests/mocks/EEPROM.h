#ifndef EEPROM_MOCK_H
#define EEPROM_MOCK_H

#include <stdint.h>
#include <map>

class EEPROMClass {
public:
    std::map<int, uint8_t> data;

    bool begin(size_t size) { return true; }
    void write(int address, uint8_t value) { data[address] = value; }
    bool commit() { return true; }
    uint8_t read(int address) { return data.count(address) ? data[address] : 0; }
    uint8_t operator[](int address) { return read(address); }
};

extern EEPROMClass EEPROM;

#endif
