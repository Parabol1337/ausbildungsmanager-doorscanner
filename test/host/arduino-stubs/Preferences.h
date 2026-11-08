#pragma once
#include <string.h>
#include "Arduino.h"

class Preferences {
public:
    bool begin(const char*, bool readOnly = false) { readOnly_ = readOnly; return !fail; }
    void end() {}
    size_t getBytesLength(const char* key) { return strcmp(key, key_) == 0 ? len_ : 0; }
    size_t getBytes(const char* key, void* buf, size_t len) { if (strcmp(key, key_) != 0 || len != len_) return 0; memcpy(buf, data_, len); return len; }
    size_t putBytes(const char* key, const void* buf, size_t len) { if (readOnly_ || len > sizeof(data_)) return 0; strcpy(key_, key); memcpy(data_, buf, len); len_ = len; return len; }
    bool remove(const char* key) { if (strcmp(key, key_) == 0) { len_ = 0; key_[0] = 0; return true; } return false; }
    static bool fail;
private:
    bool readOnly_ = false;
    static char key_[16];
    static uint8_t data_[64];
    static size_t len_;
};
