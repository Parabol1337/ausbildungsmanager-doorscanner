#include "Entity.h"

#include <string.h>

namespace amlink {

namespace {
// Cuts `text` to at most `capacity - 1` bytes without splitting a UTF-8 sequence (the server rejects invalid UTF-8).
void copyUtf8(char* out, size_t capacity, const char* text) {
    size_t len = strlen(text);
    if (len > capacity - 1) {
        len = capacity - 1;
        while (len > 0 && (static_cast<unsigned char>(text[len]) & 0xC0) == 0x80) len--;
    }
    memcpy(out, text, len);
    out[len] = 0;
}
}  // namespace

bool Call::is(const char* action) const { return strcmp(action_, action) == 0; }

long Call::intArg(const char* name, long fallback) const {
    long value;
    return data_.get(name).asInt(value) ? value : fallback;
}

double Call::numberArg(const char* name, double fallback) const {
    double value;
    return data_.get(name).asDouble(value) ? value : fallback;
}

bool Call::boolArg(const char* name, bool fallback) const {
    bool value;
    return data_.get(name).asBool(value) ? value : fallback;
}

bool Call::stringArg(const char* name, char* out, size_t capacity) const { return data_.get(name).asString(out, capacity); }

void Call::message(const char* text) { copyUtf8(message_, sizeof(message_), text); }

void Entity::clear() {
    key_ = domain_ = label_ = class_ = category_ = nullptr;
    featureCount_ = 0;
    attrCount_ = 0;
    handler_ = nullptr;
    context_ = nullptr;
    state_[0] = 0;
    hasState_ = false;
    numeric_ = false;
}

Entity& Entity::feature(const char* name) {
    if (featureCount_ < AMLINK_MAX_FEATURES) features_[featureCount_++] = name;
    return *this;
}

Entity& Entity::attr(const char* name, long value) {
    if (attrCount_ < AMLINK_MAX_ATTRS) {
        Attr& a = attrs_[attrCount_++];
        a.name = name; a.kind = AttrInt; a.number = value; a.text = nullptr; a.list = nullptr;
    }
    return *this;
}

Entity& Entity::attr(const char* name, const char* value) {
    if (attrCount_ < AMLINK_MAX_ATTRS) {
        Attr& a = attrs_[attrCount_++];
        a.name = name; a.kind = AttrString; a.number = 0; a.text = value; a.list = nullptr;
    }
    return *this;
}

Entity& Entity::attrBool(const char* name, bool value) {
    if (attrCount_ < AMLINK_MAX_ATTRS) {
        Attr& a = attrs_[attrCount_++];
        a.name = name; a.kind = AttrBool; a.number = value ? 1 : 0; a.text = nullptr; a.list = nullptr;
    }
    return *this;
}

Entity& Entity::options(const char* const* list, uint8_t count) {
    if (attrCount_ < AMLINK_MAX_ATTRS) {
        Attr& a = attrs_[attrCount_++];
        a.name = "options"; a.kind = AttrOptions; a.number = count; a.text = nullptr; a.list = list;
    }
    return *this;
}

}  // namespace amlink
