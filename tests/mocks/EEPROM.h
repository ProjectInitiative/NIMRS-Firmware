#ifndef EEPROM_MOCK_H
#define EEPROM_MOCK_H

#include <vector>
#include <stdint.h>
#include <stddef.h>

class EEPROMClass {
public:
    std::vector<uint8_t> data;

    EEPROMClass() : data(4096, 0xFF) {} // Default erased state

    bool begin(size_t size) {
        if (size > data.size()) data.resize(size, 0xFF);
        return true;
    }

    uint8_t read(int address) {
        if (address < 0 || (size_t)address >= data.size()) return 0xFF;
        return data[address];
    }

    void write(int address, uint8_t value) {
        if (address >= 0 && (size_t)address < data.size()) {
            data[address] = value;
        }
    }

    void update(int address, uint8_t value) {
        write(address, value);
    }

    bool commit() { return true; }

    uint8_t operator[](int const address) {
        return read(address);
    }
};

extern EEPROMClass EEPROM;

#endif
