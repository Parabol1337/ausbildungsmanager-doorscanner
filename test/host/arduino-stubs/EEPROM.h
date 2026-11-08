#pragma once
#include <string.h>
#include "Arduino.h"

class EEPROMClass {
public:
    EEPROMClass() : commitOk(true), commits(0), size_(0) { memset(flash_, 0xff, sizeof(flash_)); memcpy(ram_, flash_, sizeof(ram_)); }
    void begin(size_t size) { size_ = size; memcpy(ram_, flash_, sizeof(ram_)); }
    uint8_t read(int address) { return (size_t)address < size_ ? ram_[address] : 0xff; }
    void write(int address, uint8_t value) { if ((size_t)address < size_) ram_[address] = value; }
    bool commit() { commits++; if (!commitOk) return false; memcpy(flash_, ram_, sizeof(flash_)); return true; }
    // test helpers
    void powerCycle() { memcpy(ram_, flash_, sizeof(ram_)); size_ = 0; }
    uint8_t* flash() { return flash_; }
    bool commitOk;
    int commits;
private:
    size_t size_;
    uint8_t flash_[512], ram_[512];
};
extern EEPROMClass EEPROM;
