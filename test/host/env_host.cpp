// The environment of the RP2040 for the host build (the real one uses the chip's random generator and unique id).
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "amlink/Env.h"

namespace amlink {
namespace {
class HostRp2040Env : public Env {
public:
    uint32_t millis() override { timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return uint32_t(ts.tv_sec) * 1000u + uint32_t(ts.tv_nsec / 1000000); }
    void random(uint8_t* out, size_t length) override {
        int fd = open("/dev/urandom", O_RDONLY);
        size_t got = 0;
        while (fd >= 0 && got < length) { ssize_t n = read(fd, out + got, length - got); if (n <= 0) break; got += size_t(n); }
        if (fd >= 0) close(fd);
        while (got < length) out[got++] = 0;
    }
    size_t hardwareId(char* out, size_t capacity) override { return size_t(snprintf(out, capacity, "e6616407%08x", unsigned(getpid()))); }
    const char* platform() const override { return "rp2040"; }
};
}  // namespace
Env& defaultEnv() { static HostRp2040Env env; return env; }
}  // namespace amlink
