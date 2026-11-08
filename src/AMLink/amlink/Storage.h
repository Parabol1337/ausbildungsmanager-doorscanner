#pragma once
#include "Config.h"

namespace amlink {

/** Small persistent key/value store for the device secret. Must survive power cycles and firmware updates. */
class Storage {
public:
    virtual ~Storage() {}
    /** Reads exactly `length` bytes stored under `name`; false if missing or the size differs. */
    virtual bool read(const char* name, uint8_t* buffer, size_t length) = 0;
    /** Stores and commits `length` bytes. Returns false if the data could not be made persistent. */
    virtual bool write(const char* name, const uint8_t* data, size_t length) = 0;
    virtual void erase(const char* name) = 0;
};

/** RAM only: for tests. The identity is lost on reset. */
class MemoryStorage : public Storage {
public:
    MemoryStorage() : length_(0) { name_[0] = 0; }
    bool read(const char* name, uint8_t* buffer, size_t length) override;
    bool write(const char* name, const uint8_t* data, size_t length) override;
    void erase(const char* name) override;

private:
    char name_[16];
    uint8_t data_[64];
    size_t length_;
};

}  // namespace amlink
