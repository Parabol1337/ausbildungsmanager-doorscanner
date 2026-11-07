// Runs the REAL, unmodified RP2040-ETH_Door.ino on a PC. The runner plays the hardware around it:
//  - the CH9120 network bridge: a TCP connection to the server, delivered to the sketch as serial data on Serial2,
//    with the "not connected" pin (D17) following the connection (and reconnecting like the chip does)
//  - the Wiegand reader, the doorbell and the door contact: injected through commands on stdin
//  - the door opener output (D2): reported on stdout
//
//   host_test <server-host> <server-port>
// stdin: scan <hex> | key <digit|esc|enter> | bell | button | release | door-open | door-closed | quit
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "Arduino.h"
#include "Stream.h"
#include "EEPROM.h"
#include "stubs/CH9120.h"
#include "stubs/Wiegand.h"

int g_pinLevel[64];
uint32_t g_millis = 0;
bool BOOTSEL = false;
EEPROMClass EEPROM;
void CH9120_init(void) {}

namespace {
int g_fd = -1;
const char* g_host;
int g_port;
uint32_t g_retryAt = 0;

uint32_t nowMs() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return uint32_t(ts.tv_sec) * 1000u + uint32_t(ts.tv_nsec / 1000000);
}

struct StdoutSerial : HardwareSerial {
    size_t write(uint8_t b) override { fputc(b, stdout); if (b == '\n') fflush(stdout); return 1; }
};

// UART2 as the sketch sees it: bytes from and to the TCP connection held by the "chip".
struct BridgeSerial : HardwareSerial {
    size_t write(uint8_t b) override { return write(&b, 1); }
    size_t write(const uint8_t* data, size_t length) override {
        if (g_fd < 0) return 0;
        ssize_t n = send(g_fd, data, length, MSG_NOSIGNAL | MSG_DONTWAIT);
        return n > 0 ? size_t(n) : 0;
    }
    int available() override {
        if (g_fd < 0) return 0;
        int n = 0;
        ioctl_available(n);
        return n;
    }
    int read() override {
        uint8_t b;
        if (g_fd < 0) return -1;
        ssize_t n = recv(g_fd, &b, 1, MSG_DONTWAIT);
        return n == 1 ? b : -1;
    }
    int peek() override { return -1; }
    void ioctl_available(int& n);
};

StdoutSerial g_stdout;
BridgeSerial g_bridge;
}  // namespace

HardwareSerial& Serial = g_stdout;
HardwareSerial& Serial1 = g_stdout;
HardwareSerial& Serial2 = g_bridge;

#include <sys/ioctl.h>
void BridgeSerial::ioctl_available(int& n) { ::ioctl(g_fd, FIONREAD, &n); }

// The chip: connect to the server, report the state on the "not connected" pin, reconnect when the link is lost.
static void bridgeStep(uint32_t now) {
    if (g_fd >= 0) {
        pollfd p = {g_fd, POLLIN, 0};
        if (poll(&p, 1, 0) > 0 && (p.revents & (POLLIN | POLLHUP | POLLERR))) {
            char c;
            if (recv(g_fd, &c, 1, MSG_PEEK | MSG_DONTWAIT) == 0) {   // server closed
                close(g_fd);
                g_fd = -1;
                g_retryAt = now + 300;
            }
        }
    } else if (static_cast<int32_t>(now - g_retryAt) >= 0) {
        addrinfo hints = {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* res = nullptr;
        char port[8];
        snprintf(port, sizeof(port), "%d", g_port);
        if (getaddrinfo(g_host, port, &hints, &res) == 0) {
            int fd = socket(res->ai_family, res->ai_socktype, 0);
            if (fd >= 0 && connect(fd, res->ai_addr, res->ai_addrlen) == 0) {
                int one = 1;
                setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
                fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
                g_fd = fd;
            } else if (fd >= 0) {
                close(fd);
            }
            freeaddrinfo(res);
        }
        if (g_fd < 0) g_retryAt = now + 300;
    }
    g_pinLevel[D17] = g_fd >= 0 ? LOW : HIGH;   // the CH9120 pin is HIGH while not connected
}

// ---- the real sketch -----------------------------------------------------------------------------------------------
#include "../../RP2040-ETH_Door.ino"
// ---------------------------------------------------------------------------------------------------------------------

static void command(char* line) {
    char* arg = strchr(line, ' ');
    if (arg) *arg++ = 0;
    if (!strcmp(line, "scan") && arg) {
        WG_Scanner.push(strtoul(arg, nullptr, 16), 34);
    } else if (!strcmp(line, "key") && arg) {
        if (!strcmp(arg, "esc")) WG_Scanner.push(27, 4);
        else if (!strcmp(arg, "enter")) WG_Scanner.push(13, 4);
        else WG_Scanner.push(strtoul(arg, nullptr, 10), 4);
    } else if (!strcmp(line, "bell")) {
        g_pinLevel[Bell_In] = LOW;
    } else if (!strcmp(line, "bell-release")) {
        g_pinLevel[Bell_In] = HIGH;
    } else if (!strcmp(line, "door-open")) {
        g_pinLevel[Door_In] = HIGH;
    } else if (!strcmp(line, "door-closed")) {
        g_pinLevel[Door_In] = LOW;
    } else if (!strcmp(line, "button")) {
        BOOTSEL = true;
    } else if (!strcmp(line, "release")) {
        BOOTSEL = false;
    } else if (!strcmp(line, "quit")) {
        printf("BYE\n");
        fflush(stdout);
        exit(0);
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s host port\n", argv[0]);
        return 2;
    }
    for (int fd = 3; fd < 256; fd++) close(fd);   // sockets inherited from the test harness
    g_host = argv[1];
    g_port = atoi(argv[2]);
    g_pinLevel[Bell_In] = HIGH;   // INPUT_PULLUP: not pressed
    g_pinLevel[Door_In] = LOW;    // closed
    g_pinLevel[D17] = HIGH;

    g_millis = nowMs();
    setup();
    printf("READY\n");
    fflush(stdout);

    int lastOpener = g_pinLevel[Door_Out];
    char buf[256];
    size_t used = 0;
    for (;;) {
        g_millis = nowMs();
        bridgeStep(g_millis);
        loop();
        if (g_pinLevel[Door_Out] != lastOpener) {
            lastOpener = g_pinLevel[Door_Out];
            printf("OPENER %s\n", lastOpener ? "on" : "off");
            fflush(stdout);
        }
        pollfd p = {0, POLLIN, 0};
        if (poll(&p, 1, 2) > 0 && (p.revents & POLLIN)) {
            ssize_t n = read(0, buf + used, sizeof(buf) - used - 1);
            if (n == 0) return 0;
            if (n > 0) {
                used += size_t(n);
                buf[used] = 0;
                for (;;) {
                    char* nl = strchr(buf, '\n');
                    if (!nl) break;
                    *nl = 0;
                    command(buf);
                    size_t consumed = size_t(nl + 1 - buf);
                    memmove(buf, buf + consumed, used - consumed + 1);
                    used -= consumed;
                }
            }
        }
    }
}
