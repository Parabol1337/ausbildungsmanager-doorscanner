#pragma once
#include "Config.h"

namespace amlink {

/** SHA-256 (FIPS 180-4). Streaming, no dynamic memory. */
class Sha256 {
public:
    static const size_t DigestSize = 32;
    static const size_t BlockSize = 64;

    Sha256() { reset(); }
    void reset();
    void update(const uint8_t* data, size_t len);
    void update(const char* text);
    void final(uint8_t out[DigestSize]);

private:
    void block(const uint8_t* chunk);
    uint32_t h_[8];
    uint8_t buffer_[BlockSize];
    uint64_t total_;
    size_t fill_;
};

/** HMAC-SHA256 (RFC 2104). Copying an instance after init() reuses the keyed state. */
class Hmac {
public:
    Hmac() : ready_(false) {}
    void init(const uint8_t* key, size_t keyLen);
    /** Starts a new message with the key given to init(). */
    void begin();
    void update(const uint8_t* data, size_t len) { inner_.update(data, len); }
    void update(const char* text) { inner_.update(text); }
    void final(uint8_t out[Sha256::DigestSize]);

    /** One-shot helper. */
    static void compute(const uint8_t* key, size_t keyLen, const uint8_t* data, size_t len, uint8_t out[Sha256::DigestSize]);

private:
    Sha256 inner_;
    uint8_t opad_[Sha256::BlockSize];
    uint8_t ipad_[Sha256::BlockSize];
    bool ready_;
};

/** Lower-case hex encoding; `out` needs 2*len+1 bytes. */
void toHex(const uint8_t* data, size_t len, char* out);
/** Decodes exactly `len` bytes from 2*len hex characters (either case). */
bool fromHex(const char* hex, size_t hexLen, uint8_t* out, size_t len);
/** Constant-time comparison. */
bool constantTimeEquals(const uint8_t* a, const uint8_t* b, size_t len);

}  // namespace amlink
