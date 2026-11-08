#include "Json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace amlink {

namespace {

const int MaxDepth = 8;

inline bool isWs(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
inline const char* skipWs(const char* p, const char* end) {
    while (p < end && isWs(*p)) p++;
    return p;
}
inline bool isHex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
inline uint32_t hexVal(char c) { return c <= '9' ? uint32_t(c - '0') : uint32_t((c | 0x20) - 'a' + 10); }

// Position after the closing quote, or nullptr if invalid. `p` points at the opening quote.
const char* skipString(const char* p, const char* end) {
    if (p >= end || *p != '"') return nullptr;
    p++;
    while (p < end) {
        unsigned char c = static_cast<unsigned char>(*p);
        if (c == '"') return p + 1;
        if (c < 0x20) return nullptr;
        if (c == '\\') {
            p++;
            if (p >= end) return nullptr;
            char e = *p;
            if (e == 'u') {
                if (end - p < 5 || !isHex(p[1]) || !isHex(p[2]) || !isHex(p[3]) || !isHex(p[4])) return nullptr;
                p += 5;
                continue;
            }
            if (!(e == '"' || e == '\\' || e == '/' || e == 'b' || e == 'f' || e == 'n' || e == 'r' || e == 't')) return nullptr;
        }
        p++;
    }
    return nullptr;
}

const char* skipNumber(const char* p, const char* end) {
    if (p < end && *p == '-') p++;
    if (p >= end) return nullptr;
    if (*p == '0') {
        p++;
    } else if (*p >= '1' && *p <= '9') {
        while (p < end && *p >= '0' && *p <= '9') p++;
    } else {
        return nullptr;
    }
    if (p < end && *p == '.') {
        p++;
        if (p >= end || *p < '0' || *p > '9') return nullptr;
        while (p < end && *p >= '0' && *p <= '9') p++;
    }
    if (p < end && (*p == 'e' || *p == 'E')) {
        p++;
        if (p < end && (*p == '+' || *p == '-')) p++;
        if (p >= end || *p < '0' || *p > '9') return nullptr;
        while (p < end && *p >= '0' && *p <= '9') p++;
    }
    return p;
}

bool matchLiteral(const char*& p, const char* end, const char* lit) {
    size_t n = strlen(lit);
    if (static_cast<size_t>(end - p) < n || memcmp(p, lit, n) != 0) return false;
    p += n;
    return true;
}

// Position after the value starting at `p` (no leading whitespace), or nullptr when invalid.
const char* skipValue(const char* p, const char* end, int depth) {
    if (p >= end || depth > MaxDepth) return nullptr;
    switch (*p) {
        case '"':
            return skipString(p, end);
        case '{': {
            p = skipWs(p + 1, end);
            if (p < end && *p == '}') return p + 1;
            for (;;) {
                p = skipString(p, end);
                if (!p) return nullptr;
                p = skipWs(p, end);
                if (p >= end || *p != ':') return nullptr;
                p = skipWs(p + 1, end);
                p = skipValue(p, end, depth + 1);
                if (!p) return nullptr;
                p = skipWs(p, end);
                if (p >= end) return nullptr;
                if (*p == '}') return p + 1;
                if (*p != ',') return nullptr;
                p = skipWs(p + 1, end);
            }
        }
        case '[': {
            p = skipWs(p + 1, end);
            if (p < end && *p == ']') return p + 1;
            for (;;) {
                p = skipValue(p, end, depth + 1);
                if (!p) return nullptr;
                p = skipWs(p, end);
                if (p >= end) return nullptr;
                if (*p == ']') return p + 1;
                if (*p != ',') return nullptr;
                p = skipWs(p + 1, end);
            }
        }
        case 't': return matchLiteral(p, end, "true") ? p : nullptr;
        case 'f': return matchLiteral(p, end, "false") ? p : nullptr;
        case 'n': return matchLiteral(p, end, "null") ? p : nullptr;
        default: return skipNumber(p, end);
    }
}

// Decodes the next character of a validated string body. `p` is advanced.
//  - at the closing quote: returns false with bad=false
//  - lone surrogate: returns false with bad=true
//  - raw byte (incl. UTF-8 bytes copied as they are): cp = byte, isCodePoint=false
//  - escape sequence: cp = code point,                   isCodePoint=true (needs UTF-8 encoding)
bool nextChar(const char*& p, uint32_t& cp, bool& isCodePoint, bool& bad) {
    bad = false;
    isCodePoint = false;
    if (*p == '"') return false;
    if (*p != '\\') {
        cp = static_cast<unsigned char>(*p++);
        return true;
    }
    p++;
    char e = *p++;
    isCodePoint = true;
    switch (e) {
        case 'b': cp = '\b'; return true;
        case 'f': cp = '\f'; return true;
        case 'n': cp = '\n'; return true;
        case 'r': cp = '\r'; return true;
        case 't': cp = '\t'; return true;
        case 'u': {
            uint32_t u = 0;
            for (int i = 0; i < 4; i++) u = (u << 4) | hexVal(p[i]);
            p += 4;
            if (u >= 0xD800 && u <= 0xDBFF) {
                if (p[0] == '\\' && p[1] == 'u' && isHex(p[2]) && isHex(p[3]) && isHex(p[4]) && isHex(p[5])) {
                    uint32_t low = 0;
                    for (int i = 0; i < 4; i++) low = (low << 4) | hexVal(p[2 + i]);
                    if (low >= 0xDC00 && low <= 0xDFFF) {
                        p += 6;
                        cp = 0x10000 + ((u - 0xD800) << 10) + (low - 0xDC00);
                        return true;
                    }
                }
                bad = true;
                return false;
            }
            if (u >= 0xDC00 && u <= 0xDFFF) {
                bad = true;
                return false;
            }
            cp = u;
            return true;
        }
        default:
            cp = static_cast<unsigned char>(e);  // " \ /
            return true;
    }
}

size_t encodeUtf8(uint32_t cp, char* out) {
    if (cp < 0x80) { out[0] = char(cp); return 1; }
    if (cp < 0x800) { out[0] = char(0xC0 | (cp >> 6)); out[1] = char(0x80 | (cp & 0x3f)); return 2; }
    if (cp < 0x10000) { out[0] = char(0xE0 | (cp >> 12)); out[1] = char(0x80 | ((cp >> 6) & 0x3f)); out[2] = char(0x80 | (cp & 0x3f)); return 3; }
    out[0] = char(0xF0 | (cp >> 18));
    out[1] = char(0x80 | ((cp >> 12) & 0x3f));
    out[2] = char(0x80 | ((cp >> 6) & 0x3f));
    out[3] = char(0x80 | (cp & 0x3f));
    return 4;
}

}  // namespace

bool JsonValue::parse(const char* s, size_t n, JsonValue& out) {
    const char* end = s + n;
    const char* p = skipWs(s, end);
    const char* stop = skipValue(p, end, 0);
    if (!stop || skipWs(stop, end) != end) return false;
    out = JsonValue(p, static_cast<size_t>(stop - p));
    return true;
}

JsonValue JsonValue::get(const char* key) const {
    if (!isObject()) return JsonValue();
    const char* end = p_ + n_;
    const char* p = skipWs(p_ + 1, end);
    size_t keyLen = strlen(key);
    while (p < end && *p == '"') {
        const char* keyStart = p;
        const char* keyEnd = skipString(p, end);
        p = skipWs(skipWs(keyEnd, end) + 1, end);  // past ':'
        const char* valueEnd = skipValue(p, end, 0);
        bool match;
        if (memchr(keyStart, '\\', static_cast<size_t>(keyEnd - keyStart)) == nullptr) {
            match = static_cast<size_t>(keyEnd - keyStart) == keyLen + 2 && memcmp(keyStart + 1, key, keyLen) == 0;
        } else {
            match = JsonValue(keyStart, static_cast<size_t>(keyEnd - keyStart)).equals(key);
        }
        if (match) return JsonValue(p, static_cast<size_t>(valueEnd - p));
        p = skipWs(valueEnd, end);
        if (p < end && *p == ',') p = skipWs(p + 1, end);
        else break;
    }
    return JsonValue();
}

JsonValue JsonValue::at(size_t index) const {
    if (!isArray()) return JsonValue();
    const char* end = p_ + n_;
    const char* p = skipWs(p_ + 1, end);
    for (size_t i = 0; p < end && *p != ']'; i++) {
        const char* valueEnd = skipValue(p, end, 0);
        if (i == index) return JsonValue(p, static_cast<size_t>(valueEnd - p));
        p = skipWs(valueEnd, end);
        if (p < end && *p == ',') p = skipWs(p + 1, end);
    }
    return JsonValue();
}

size_t JsonValue::size() const {
    if (!isArray() && !isObject()) return 0;
    const char* end = p_ + n_;
    const char* p = skipWs(p_ + 1, end);
    size_t count = 0;
    const char close = isArray() ? ']' : '}';
    const bool object = isObject();
    while (p < end && *p != close) {
        if (object) p = skipWs(skipWs(skipString(p, end), end) + 1, end);
        p = skipWs(skipValue(p, end, 0), end);
        count++;
        if (p < end && *p == ',') p = skipWs(p + 1, end);
    }
    return count;
}

bool JsonValue::asString(char* out, size_t capacity) const {
    if (capacity == 0) return false;
    out[0] = 0;
    if (!isString()) return false;
    const char* p = p_ + 1;
    size_t len = 0;
    for (;;) {
        uint32_t cp;
        bool isCodePoint, bad;
        if (!nextChar(p, cp, isCodePoint, bad)) {
            out[len] = 0;
            return !bad;
        }
        char tmp[4];
        size_t tn;
        if (isCodePoint) {
            tn = encodeUtf8(cp, tmp);
        } else {
            tmp[0] = char(cp);
            tn = 1;
        }
        if (len + tn + 1 > capacity) {
            out[0] = 0;
            return false;
        }
        memcpy(out + len, tmp, tn);
        len += tn;
    }
}

bool JsonValue::equals(const char* ascii) const {
    if (!isString()) return false;
    const char* p = p_ + 1;
    for (;;) {
        uint32_t cp;
        bool isCodePoint, bad;
        if (!nextChar(p, cp, isCodePoint, bad)) return !bad && *ascii == 0;
        if (*ascii == 0 || static_cast<unsigned char>(*ascii) != cp) return false;
        ascii++;
    }
}

bool JsonValue::asBool(bool& out) const {
    if (!isBool()) return false;
    out = (*p_ == 't');
    return true;
}

bool JsonValue::asInt(long& out) const {
    if (!isNumber()) return false;
    const char* p = p_;
    const char* end = p_ + n_;
    bool negative = false;
    if (*p == '-') { negative = true; p++; }
    const unsigned long long limit = negative ? 2147483648ULL : 2147483647ULL;
    unsigned long long magnitude = 0;
    for (; p < end; p++) {
        if (*p < '0' || *p > '9') return false;  // fraction or exponent: not an integer
        magnitude = magnitude * 10 + static_cast<unsigned>(*p - '0');
        if (magnitude > limit) return false;
    }
    out = negative ? static_cast<long>(-static_cast<long long>(magnitude)) : static_cast<long>(magnitude);
    return true;
}

bool JsonValue::asDouble(double& out) const {
    if (!isNumber() || n_ > 40) return false;
    char tmp[41];
    memcpy(tmp, p_, n_);
    tmp[n_] = 0;
    out = strtod(tmp, nullptr);
    return true;
}

// ---------------------------------------------------------------------------------------------------------

JsonWriter::JsonWriter(char* buffer, size_t capacity)
    : buffer_(buffer), capacity_(capacity), length_(0), overflow_(capacity == 0), afterKey_(false), depth_(0) {
    for (size_t i = 0; i < sizeof(needComma_); i++) needComma_[i] = false;
}

void JsonWriter::put(char c) { put(&c, 1); }

void JsonWriter::put(const char* text, size_t length) {
    if (overflow_) return;
    if (length_ + length + 1 > capacity_) {  // one byte stays free for the NUL
        overflow_ = true;
        return;
    }
    memcpy(buffer_ + length_, text, length);
    length_ += length;
}

void JsonWriter::beforeValue() {
    if (afterKey_) {
        afterKey_ = false;
        return;
    }
    if (needComma_[depth_]) put(',');
    needComma_[depth_] = true;
}

void JsonWriter::quoted(const char* text, size_t length) {
    put('"');
    for (size_t i = 0; i < length; i++) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        switch (c) {
            case '"': put("\\\"", 2); break;
            case '\\': put("\\\\", 2); break;
            case '\n': put("\\n", 2); break;
            case '\r': put("\\r", 2); break;
            case '\t': put("\\t", 2); break;
            case '\b': put("\\b", 2); break;
            case '\f': put("\\f", 2); break;
            default:
                if (c < 0x20) {
                    static const char digits[] = "0123456789abcdef";
                    char esc[6] = {'\\', 'u', '0', '0', digits[c >> 4], digits[c & 15]};
                    put(esc, 6);
                } else {
                    put(static_cast<char>(c));  // UTF-8 bytes are passed through unchanged
                }
        }
    }
    put('"');
}

JsonWriter& JsonWriter::beginObject() {
    beforeValue();
    put('{');
    if (depth_ < 9) depth_++;
    needComma_[depth_] = false;
    return *this;
}

JsonWriter& JsonWriter::endObject() {
    put('}');
    if (depth_ > 0) depth_--;
    return *this;
}

JsonWriter& JsonWriter::beginArray() {
    beforeValue();
    put('[');
    if (depth_ < 9) depth_++;
    needComma_[depth_] = false;
    return *this;
}

JsonWriter& JsonWriter::endArray() {
    put(']');
    if (depth_ > 0) depth_--;
    return *this;
}

JsonWriter& JsonWriter::key(const char* name) {
    if (needComma_[depth_]) put(',');
    needComma_[depth_] = true;
    quoted(name, strlen(name));
    put(':');
    afterKey_ = true;
    return *this;
}

JsonWriter& JsonWriter::str(const char* value) { return str(value, strlen(value)); }

JsonWriter& JsonWriter::str(const char* value, size_t length) {
    beforeValue();
    quoted(value, length);
    return *this;
}

JsonWriter& JsonWriter::num(long value) {
    beforeValue();
    char tmp[24];
    int n = snprintf(tmp, sizeof(tmp), "%ld", value);
    put(tmp, static_cast<size_t>(n));
    return *this;
}

JsonWriter& JsonWriter::num(double value) {
    beforeValue();
    char tmp[32];
    int n = snprintf(tmp, sizeof(tmp), "%.6g", value);
    // JSON has no NaN / Infinity
    if (n <= 0 || strchr(tmp, 'n') != nullptr || strchr(tmp, 'N') != nullptr) {
        put("null", 4);
    } else {
        put(tmp, static_cast<size_t>(n));
    }
    return *this;
}

JsonWriter& JsonWriter::boolean(bool value) {
    beforeValue();
    if (value) put("true", 4); else put("false", 5);
    return *this;
}

JsonWriter& JsonWriter::null() {
    beforeValue();
    put("null", 4);
    return *this;
}

JsonWriter& JsonWriter::raw(const char* json, size_t length) {
    beforeValue();
    put(json, length);
    return *this;
}

JsonWriter& JsonWriter::append(const char* text, size_t length) {
    put(text, length);
    return *this;
}

const char* JsonWriter::c_str() {
    if (capacity_ > 0) buffer_[overflow_ ? 0 : length_] = 0;
    return buffer_;
}

}  // namespace amlink
