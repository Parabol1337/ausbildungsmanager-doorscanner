#pragma once
// Stores the device secret in the ESP32's non-volatile storage (NVS) through the Preferences library.
#if defined(ARDUINO_ARCH_ESP32) || defined(AMLINK_ARDUINO_STUBS)

#include <Preferences.h>

#include "../Storage.h"

namespace amlink {

class PreferencesStorage : public Storage {
public:
    explicit PreferencesStorage(const char* space = "amlink") : space_(space) {}

    bool read(const char* name, uint8_t* buffer, size_t length) override {
        Preferences p;
        if (!p.begin(space_, true)) return false;
        bool ok = p.getBytesLength(name) == length && p.getBytes(name, buffer, length) == length;
        p.end();
        return ok;
    }
    bool write(const char* name, const uint8_t* data, size_t length) override {
        Preferences p;
        if (!p.begin(space_, false)) return false;
        bool ok = p.putBytes(name, data, length) == length;
        p.end();
        return ok;
    }
    void erase(const char* name) override {
        Preferences p;
        if (p.begin(space_, false)) {
            p.remove(name);
            p.end();
        }
    }

private:
    const char* space_;
};

}  // namespace amlink

#endif
