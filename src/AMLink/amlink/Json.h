#pragma once
#include "Config.h"

namespace amlink {

/**
 * Read-only view into a JSON text. No allocation, no copies: a value is a slice of the original buffer,
 * which must stay alive and unchanged while values are used.
 *
 * parse() validates the whole document strictly once (RFC 8259, nesting limited to 8); all accessors
 * can therefore walk the text without further checks. Duplicate keys: the first one wins.
 */
class JsonValue {
public:
    JsonValue() : p_(nullptr), n_(0) {}

    /** Validates `s[0..n)` and returns its root value. */
    static bool parse(const char* s, size_t n, JsonValue& out);

    bool valid() const { return p_ != nullptr; }
    bool isObject() const { return valid() && *p_ == '{'; }
    bool isArray() const { return valid() && *p_ == '['; }
    bool isString() const { return valid() && *p_ == '"'; }
    bool isBool() const { return valid() && (*p_ == 't' || *p_ == 'f'); }
    bool isNull() const { return valid() && *p_ == 'n'; }
    bool isNumber() const { return valid() && (*p_ == '-' || (*p_ >= '0' && *p_ <= '9')); }

    /** Member of an object, or an invalid value if missing / not an object. */
    JsonValue get(const char* key) const;
    /** Element of an array, or an invalid value. O(index). */
    JsonValue at(size_t index) const;
    /** Number of array elements or object members (0 for other types). */
    size_t size() const;

    /** Decodes a string (escapes, \uXXXX, surrogate pairs -> UTF-8) into `out`, NUL-terminated. */
    bool asString(char* out, size_t capacity) const;
    /** Compares a string value with an ASCII literal. */
    bool equals(const char* ascii) const;
    bool asBool(bool& out) const;
    /** Integer syntax only (no fraction / exponent), range -2^31 .. 2^31-1. */
    bool asInt(long& out) const;
    bool asDouble(double& out) const;

    const char* raw() const { return p_; }
    size_t rawLength() const { return n_; }

private:
    JsonValue(const char* p, size_t n) : p_(p), n_(n) {}
    const char* p_;
    size_t n_;
};

/**
 * Writes JSON into a caller-provided buffer. Commas are inserted automatically. When the buffer is too
 * small, ok() turns false and the content must be discarded.
 */
class JsonWriter {
public:
    JsonWriter(char* buffer, size_t capacity);

    JsonWriter& beginObject();
    JsonWriter& endObject();
    JsonWriter& beginArray();
    JsonWriter& endArray();
    JsonWriter& key(const char* name);
    JsonWriter& str(const char* value);
    JsonWriter& str(const char* value, size_t length);
    JsonWriter& num(long value);
    JsonWriter& num(double value);
    JsonWriter& boolean(bool value);
    JsonWriter& null();
    /** Appends already valid JSON as one value (e.g. a pre-rendered object). */
    JsonWriter& raw(const char* json, size_t length);
    /** Appends bytes that are not a value (separators, signature trailer). Does not touch comma state. */
    JsonWriter& append(const char* text, size_t length);

    bool ok() const { return !overflow_; }
    size_t size() const { return length_; }
    char* data() { return buffer_; }
    /** NUL-terminates (there is always room) and returns the buffer. */
    const char* c_str();

private:
    void beforeValue();
    void put(char c);
    void put(const char* text, size_t length);
    void quoted(const char* text, size_t length);

    char* buffer_;
    size_t capacity_;
    size_t length_;
    bool overflow_;
    bool afterKey_;
    uint8_t depth_;
    bool needComma_[10];
};

}  // namespace amlink
