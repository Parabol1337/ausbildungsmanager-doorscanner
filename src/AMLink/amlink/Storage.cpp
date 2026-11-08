#include "Storage.h"

#include <string.h>

namespace amlink {

bool MemoryStorage::read(const char* name, uint8_t* buffer, size_t length) {
    if (length_ != length || strcmp(name_, name) != 0) return false;
    memcpy(buffer, data_, length);
    return true;
}

bool MemoryStorage::write(const char* name, const uint8_t* data, size_t length) {
    if (length > sizeof(data_) || strlen(name) >= sizeof(name_)) return false;
    strcpy(name_, name);
    memcpy(data_, data, length);
    length_ = length;
    return true;
}

void MemoryStorage::erase(const char* name) {
    if (strcmp(name_, name) == 0) {
        length_ = 0;
        name_[0] = 0;
    }
}

}  // namespace amlink
