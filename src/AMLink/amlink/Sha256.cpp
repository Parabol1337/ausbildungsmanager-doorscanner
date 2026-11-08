#include "Sha256.h"

#include <string.h>

namespace amlink {

namespace {
const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }
}  // namespace

void Sha256::reset() {
    static const uint32_t init[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(h_, init, sizeof(h_));
    total_ = 0;
    fill_ = 0;
}

void Sha256::block(const uint8_t* p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = (uint32_t(p[i * 4]) << 24) | (uint32_t(p[i * 4 + 1]) << 16) | (uint32_t(p[i * 4 + 2]) << 8) | p[i * 4 + 3];
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4], f = h_[5], g = h_[6], h = h_[7];
    for (int i = 0; i < 64; i++) {
        uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + s1 + ch + K[i] + w[i];
        uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + maj;
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h_[0] += a; h_[1] += b; h_[2] += c; h_[3] += d; h_[4] += e; h_[5] += f; h_[6] += g; h_[7] += h;
}

void Sha256::update(const uint8_t* data, size_t len) {
    total_ += len;
    while (len > 0) {
        size_t take = BlockSize - fill_;
        if (take > len) take = len;
        memcpy(buffer_ + fill_, data, take);
        fill_ += take;
        data += take;
        len -= take;
        if (fill_ == BlockSize) {
            block(buffer_);
            fill_ = 0;
        }
    }
}

void Sha256::update(const char* text) { update(reinterpret_cast<const uint8_t*>(text), strlen(text)); }

void Sha256::final(uint8_t out[DigestSize]) {
    uint64_t bits = total_ * 8;
    uint8_t pad = 0x80;
    update(&pad, 1);
    uint8_t zero = 0;
    while (fill_ != 56) update(&zero, 1);
    uint8_t length[8];
    for (int i = 0; i < 8; i++) length[i] = uint8_t(bits >> (56 - 8 * i));
    update(length, 8);
    for (int i = 0; i < 8; i++) {
        out[i * 4] = uint8_t(h_[i] >> 24);
        out[i * 4 + 1] = uint8_t(h_[i] >> 16);
        out[i * 4 + 2] = uint8_t(h_[i] >> 8);
        out[i * 4 + 3] = uint8_t(h_[i]);
    }
    reset();
}

void Hmac::init(const uint8_t* key, size_t keyLen) {
    uint8_t k[Sha256::BlockSize];
    memset(k, 0, sizeof(k));
    if (keyLen > Sha256::BlockSize) {
        Sha256 s;
        s.update(key, keyLen);
        s.final(k);
    } else {
        memcpy(k, key, keyLen);
    }
    for (size_t i = 0; i < Sha256::BlockSize; i++) {
        ipad_[i] = k[i] ^ 0x36;
        opad_[i] = k[i] ^ 0x5c;
    }
    memset(k, 0, sizeof(k));
    ready_ = true;
    begin();
}

void Hmac::begin() {
    inner_.reset();
    if (ready_) inner_.update(ipad_, Sha256::BlockSize);
}

void Hmac::final(uint8_t out[Sha256::DigestSize]) {
    uint8_t innerHash[Sha256::DigestSize];
    inner_.final(innerHash);
    Sha256 outer;
    outer.update(opad_, Sha256::BlockSize);
    outer.update(innerHash, sizeof(innerHash));
    outer.final(out);
    begin();
}

void Hmac::compute(const uint8_t* key, size_t keyLen, const uint8_t* data, size_t len, uint8_t out[Sha256::DigestSize]) {
    Hmac h;
    h.init(key, keyLen);
    h.update(data, len);
    h.final(out);
}

void toHex(const uint8_t* data, size_t len, char* out) {
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i * 2] = digits[data[i] >> 4];
        out[i * 2 + 1] = digits[data[i] & 15];
    }
    out[len * 2] = 0;
}

static int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool fromHex(const char* hex, size_t hexLen, uint8_t* out, size_t len) {
    if (hexLen != len * 2) return false;
    for (size_t i = 0; i < len; i++) {
        int hi = hexValue(hex[i * 2]), lo = hexValue(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = uint8_t(hi << 4 | lo);
    }
    return true;
}

bool constantTimeEquals(const uint8_t* a, const uint8_t* b, size_t len) {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) diff |= uint8_t(a[i] ^ b[i]);
    return diff == 0;
}

}  // namespace amlink
