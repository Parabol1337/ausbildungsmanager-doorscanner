#pragma once
#include "Config.h"
#include "Entity.h"
#include "Env.h"
#include "Json.h"
#include "Sha256.h"
#include "Storage.h"
#include "Transport.h"

namespace amlink {

/** What this device tells the server about itself. Strings must stay valid (use literals). */
struct Identity {
    Identity(const char* model_ = "", const char* firmware_ = "", const char* deviceId_ = nullptr)
        : model(model_), firmware(firmware_), deviceId(deviceId_) {}
    const char* model;     // board or product name, e.g. "rp2040-eth-door"
    const char* firmware;  // your firmware version, e.g. "2.0.0"
    const char* deviceId;  // optional fixed id [a-z0-9_-]{4,64}; default: "<platform>-<hardware id>"
};

/** Connection progress, for status LEDs and displays. */
enum Status {
    StatusOffline,     // no connection to the server
    StatusConnecting,  // connected, logging in
    StatusPairing,     // known to the server but not approved yet: see pairingCode()
    StatusOnline       // approved and working
};

/**
 * The client side of the hardware protocol v2 (docs/hardware/README.md).
 *
 *   amlink::Link amLink(transport, storage);       // (do not call it "link": that is a POSIX function)
 *   amLink.begin(amlink::Identity("rp2040-door", "2.0.0"));
 *   amLink.setServer("am.example.org", 8010);
 *   amLink.entity("door", "lock")->label("Tür").feature("unlock").onAction(onDoor);
 *   void loop() { amLink.loop(); ... amLink.setState("door", "locked"); }
 *
 * Single threaded: call loop() often (every few milliseconds). Nothing in the library blocks except
 * Transport::connect().
 */
class Link {
public:
    Link(Transport& transport, Storage& storage, Env& env = defaultEnv());

    /** Prepares the identity (creates the device secret on first start). Returns false if it could not be stored. */
    bool begin(const Identity& identity);
    /** Server address. Not needed with a bridge chip (SerialBridgeTransport), which connects by itself. */
    void setServer(const char* host, uint16_t port);
    /** Reconnect delays in milliseconds (defaults 2000 .. 60000). */
    void setBackoff(uint32_t minMs, uint32_t maxMs);

    /** Adds an entity. `key` must match [a-z0-9_]+(.[a-z0-9_]+)*, <= 48 characters; `domain` e.g. "lock". */
    Entity* entity(const char* key, const char* domain);
    Entity* find(const char* key);

    /** Sets the state of an entity; it is sent to the server when it changed. */
    bool setState(const char* key, const char* state);
    bool setNumber(const char* key, long value);
    bool setNumber(const char* key, double value);

    /** Reports something that happened (a card was scanned). Dropped (returns false) while offline, on purpose. */
    bool event(const char* entity, const char* name);
    bool event(const char* entity, const char* name, EventData& data);

    /** Must be called regularly. */
    void loop();

    Status status() const { return status_; }
    bool online() const { return status_ == StatusOnline; }
    const char* deviceId() const { return deviceId_; }
    /** Six digits, valid while status() == StatusPairing: compare with the one shown in the AusbildungsManager. */
    const char* pairingCode() const { return pairingCode_; }

    /** Called when status() changes. */
    void onStatus(void (*handler)(void* context, Status status), void* context = nullptr);
    /** Polled while waiting for approval: return true when the pairing button is pressed. */
    void setPairingButton(bool (*isPressed)(void* context), void* context = nullptr);
    /** Confirms the pairing (same as pressing the pairing button). */
    void confirmPairing();
    /** Forgets the device secret. The device enrols again as a new device on its next connection. */
    void factoryReset();

private:
    enum State { Idle, AwaitHello, AwaitAuth, Pending, Active, Backoff };

    // connection
    void startConnection();
    void scheduleRetry();
    void dropConnection(bool serverFault);
    void setStatus(Status status);
    uint32_t jitter(uint32_t ms);
    void pumpInput();
    void handleBytes(const uint8_t* data, size_t length);
    void handleLine(char* line, size_t length);
    bool verifySigned(char* line, size_t length, JsonValue& document);
    bool acceptSequence(const JsonValue& document);

    // handshake and signed messages
    void handleHello(const JsonValue& document);
    void handleError(const JsonValue& document);
    void handleSigned(const JsonValue& document, uint32_t receivedAt);
    void enterPending(const JsonValue& document);
    void enterActive();
    void handleCall(const JsonValue& document, uint32_t receivedAt);
    void sendAuth(const char* serverNonce);
    void sendEnroll();

    // sending
    bool begin_(JsonWriter& w, const char* type);
    bool sendPlain(JsonWriter& w);
    bool sendSigned(JsonWriter& w);
    bool writeAll(const uint8_t* data, size_t length);
    void announceEntities();
    void announceStates();
    bool sendStateOf(Entity& entity);
    void sendResult(const char* id, int code, const char* error, const char* message);
    void writeEntity(JsonWriter& w, const Entity& entity);
    void writeState(JsonWriter& w, const Entity& entity);
    void computePairingCode();
    static bool hasState(const Entity& entity);

    Transport& transport_;
    Storage& storage_;
    Env& env_;
    Identity identity_;
    char deviceId_[65];
    uint8_t secret_[32];
    bool began_;

    const char* host_;
    uint16_t port_;
    uint32_t backoffMin_, backoffMax_, backoffNow_;

    Entity entities_[AMLINK_MAX_ENTITIES];
    uint8_t entityCount_;

    State state_;
    Status status_;
    uint32_t retryAt_, stateSince_, activeSince_, lastReceived_;
    bool enrolling_;
    char serverNonce_[65];
    char deviceNonce_[33];
    Hmac sessionMac_;
    uint32_t outSeq_, inSeq_;
    char pairingCode_[8];
    bool buttonWasPressed_;

    char in_[AMLINK_LINE_SIZE];
    size_t inLength_;
    bool inOverflow_;
    char out_[AMLINK_SEND_SIZE];

    void (*statusHandler_)(void*, Status);
    void* statusContext_;
    bool (*pairingButton_)(void*);
    void* pairingButtonContext_;
};

}  // namespace amlink
