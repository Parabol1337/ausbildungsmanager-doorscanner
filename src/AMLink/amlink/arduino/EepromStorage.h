#pragma once
// Stores the device secret in the flash-backed EEPROM emulation of the RP2040 (arduino-pico) and ESP8266/ESP32 cores.
// Layout (all in one record): 'A' 'M' <length> <data...> <crc16 high> <crc16 low>.
#if defined(ARDUINO) || defined(AMLINK_ARDUINO_STUBS)

#include <EEPROM.h>

#include "../Storage.h"

namespace amlink {

class EepromStorage : public Storage {
public:
    /** `offset`: first EEPROM byte used (the record needs 5 + 32 bytes); `size`: bytes reserved for EEPROM.begin(). */
    explicit EepromStorage(size_t offset = 0, size_t size = 256) : offset_(offset), size_(size), started_(false) {}

    bool read(const char* name, uint8_t* buffer, size_t length) override {
        if (!isSecret(name) || !start() || length > MaxData) return false;
        if (EEPROM.read(int(offset_)) != 'A' || EEPROM.read(int(offset_ + 1)) != 'M') return false;
        if (EEPROM.read(int(offset_ + 2)) != length) return false;
        uint8_t data[MaxData];
        for (size_t i = 0; i < length; i++) data[i] = EEPROM.read(int(offset_ + 3 + i));
        uint16_t stored = uint16_t(EEPROM.read(int(offset_ + 3 + length)) << 8) | EEPROM.read(int(offset_ + 4 + length));
        if (stored != crc(data, length)) return false;
        for (size_t i = 0; i < length; i++) buffer[i] = data[i];
        return true;
    }

    bool write(const char* name, const uint8_t* data, size_t length) override {
        if (!isSecret(name) || !start() || length > MaxData || offset_ + length + 5 > size_) return false;
        EEPROM.write(int(offset_), 'A');
        EEPROM.write(int(offset_ + 1), 'M');
        EEPROM.write(int(offset_ + 2), uint8_t(length));
        for (size_t i = 0; i < length; i++) EEPROM.write(int(offset_ + 3 + i), data[i]);
        uint16_t sum = crc(data, length);
        EEPROM.write(int(offset_ + 3 + length), uint8_t(sum >> 8));
        EEPROM.write(int(offset_ + 4 + length), uint8_t(sum));
        return EEPROM.commit();
    }

    void erase(const char* name) override {
        if (!isSecret(name) || !start()) return;
        EEPROM.write(int(offset_), 0);
        EEPROM.commit();
    }

private:
    static const size_t MaxData = 64;
    static bool isSecret(const char* name) { return name[0] == 's' && name[1] == 'e' && name[2] == 'c' && name[3] == 'r' && name[4] == 'e' && name[5] == 't' && name[6] == 0; }
    static uint16_t crc(const uint8_t* data, size_t length) {  // CRC-16/CCITT-FALSE
        uint16_t c = 0xFFFF;
        for (size_t i = 0; i < length; i++) {
            c ^= uint16_t(data[i]) << 8;
            for (int b = 0; b < 8; b++) c = (c & 0x8000) ? uint16_t((c << 1) ^ 0x1021) : uint16_t(c << 1);
        }
        return c;
    }
    bool start() {
        if (!started_) {
            EEPROM.begin(size_);
            started_ = true;
        }
        return true;
    }
    size_t offset_, size_;
    bool started_;
};

}  // namespace amlink

#endif
