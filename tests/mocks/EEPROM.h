#ifndef EEPROM_MOCK_H
#define EEPROM_MOCK_H

#include <stdint.h>
#include <vector>
#include <cstring>

class EEPROMClass {
public:
    bool begin(size_t size) {
        _data.resize(size, 255);
        return true;
    }

    uint8_t read(int const address) {
        if(address >= 0 && (size_t)address < _data.size()) {
            return _data[address];
        }
        return 255;
    }

    void write(int const address, uint8_t const val) {
        if(address >= 0 && (size_t)address < _data.size()) {
            _data[address] = val;
        }
    }

    bool commit() { return true; }

    // For test inspection
    std::vector<uint8_t> _data;
};

// Extern declaration is usually in the library header
// In Arduino environment, EEPROM object is available.
// We will define it in mocks.cpp
extern EEPROMClass EEPROM;

#endif
