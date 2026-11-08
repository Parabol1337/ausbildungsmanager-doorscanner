#include "Link.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace amlink {

namespace {

const uint32_t HandshakeTimeoutMs = 15000;
const uint32_t IdleTimeoutMs = 90000;     // the server pings every 30 s
const uint32_t StableAfterMs = 10000;     // an online time after which the reconnect delay starts over
const uint32_t WriteTimeoutMs = 2000;
const size_t SignatureTrailer = 8 + 32 + 2;  // ,"sig":"<32 hex>"}
// Room kept free for the trailer, the newline and `,"seq":<number>`.
const size_t MessageOverhead = SignatureTrailer + 1 + 24;
const char SigMarker[] = ",\"sig\":\"";
const char* const SecretName = "secret";

bool validKey(const char* key) {
    size_t n = strlen(key);
    if (n == 0 || n > 48) return false;
    bool lastDot = true;  // no leading dot
    for (size_t i = 0; i < n; i++) {
        char c = key[i];
        if (c == '.') {
            if (lastDot) return false;
            lastDot = true;
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') {
            lastDot = false;
        } else {
            return false;
        }
    }
    return !lastDot;
}

bool validDomain(const char* domain) {
    size_t n = strlen(domain);
    if (n == 0 || n > 32) return false;
    for (size_t i = 0; i < n; i++) {
        if (!((domain[i] >= 'a' && domain[i] <= 'z') || domain[i] == '_')) return false;
    }
    return true;
}

// Domains whose entities do not have a state (docs/hardware/README.md, section 4).
bool domainHasNoState(const char* domain) {
    return strcmp(domain, "button") == 0 || strcmp(domain, "indicator") == 0 || strcmp(domain, "display") == 0;
}

const char* resultText(int code) {
    switch (code) {
        case AM_UNSUPPORTED: return "unsupported";
        case AM_BUSY: return "busy";
        case AM_INVALID: return "invalid";
        default: return "failed";
    }
}

bool isHex(const char* s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

}  // namespace

Link::Link(Transport& transport, Storage& storage, Env& env)
    : transport_(transport), storage_(storage), env_(env), identity_(), began_(false), host_(nullptr), port_(8010),
      backoffMin_(2000), backoffMax_(60000), backoffNow_(2000), entityCount_(0), state_(Idle), status_(StatusOffline),
      retryAt_(0), stateSince_(0), activeSince_(0), lastReceived_(0), enrolling_(false), outSeq_(0), inSeq_(0),
      buttonWasPressed_(false), inLength_(0), inOverflow_(false), statusHandler_(nullptr), statusContext_(nullptr),
      pairingButton_(nullptr), pairingButtonContext_(nullptr) {
    deviceId_[0] = 0;
    memset(secret_, 0, sizeof(secret_));
    serverNonce_[0] = deviceNonce_[0] = pairingCode_[0] = 0;
}

bool Link::begin(const Identity& identity) {
    // The login message carries these texts and must fit the send buffer (otherwise the device could never log in).
    if (!identity.model || !identity.firmware || strlen(identity.model) > AMLINK_MAX_MODEL ||
        strlen(identity.firmware) > AMLINK_MAX_FIRMWARE) {
        return false;
    }
    identity_ = identity;
    if (identity.deviceId && identity.deviceId[0]) {
        snprintf(deviceId_, sizeof(deviceId_), "%s", identity.deviceId);
    } else {
        char hw[40];
        size_t n = env_.hardwareId(hw, sizeof(hw));
        if (n == 0) return false;  // no way to build a stable id: give one with Identity
        snprintf(deviceId_, sizeof(deviceId_), "%s-%s", env_.platform(), hw);
    }
    // The server only accepts [a-z0-9_-]{4,64}
    size_t len = strlen(deviceId_);
    if (len < 4) return false;
    for (size_t i = 0; i < len; i++) {
        char c = deviceId_[i];
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) c = '-';
        deviceId_[i] = c;
    }

    bool have = storage_.read(SecretName, secret_, sizeof(secret_));
    bool allZero = true;
    for (size_t i = 0; i < sizeof(secret_); i++) if (secret_[i]) allZero = false;
    if (!have || allZero) {
        env_.random(secret_, sizeof(secret_));
        allZero = true;
        for (size_t i = 0; i < sizeof(secret_); i++) if (secret_[i]) allZero = false;
        // A broken random source must never produce a predictable device secret.
        if (allZero || !storage_.write(SecretName, secret_, sizeof(secret_))) return false;
    }
    began_ = true;
    return true;
}

void Link::setServer(const char* host, uint16_t port) {
    host_ = host;
    port_ = port;
}

void Link::setBackoff(uint32_t minMs, uint32_t maxMs) {
    backoffMin_ = minMs;
    backoffMax_ = maxMs < minMs ? minMs : maxMs;
    backoffNow_ = backoffMin_;
}

Entity* Link::entity(const char* key, const char* domain) {
    if (!validKey(key) || !validDomain(domain)) return nullptr;
    Entity* existing = find(key);
    if (existing) return existing;
    if (entityCount_ >= AMLINK_MAX_ENTITIES) return nullptr;
    Entity& e = entities_[entityCount_++];
    e.clear();
    e.key_ = key;
    e.domain_ = domain;
    e.numeric_ = strcmp(domain, "number") == 0;
    return &e;
}

Entity* Link::find(const char* key) {
    for (uint8_t i = 0; i < entityCount_; i++) {
        if (strcmp(entities_[i].key_, key) == 0) return &entities_[i];
    }
    return nullptr;
}

bool Link::hasState(const Entity& entity) { return entity.hasState_ && !domainHasNoState(entity.domain_); }

bool Link::setState(const char* key, const char* state) {
    Entity* e = find(key);
    if (!e || e->numeric_ || domainHasNoState(e->domain_)) return false;
    char next[sizeof(e->state_)];
    // Copy without splitting a UTF-8 character
    size_t len = strlen(state);
    if (len > sizeof(next) - 1) {
        len = sizeof(next) - 1;
        while (len > 0 && (static_cast<unsigned char>(state[len]) & 0xC0) == 0x80) len--;
    }
    memcpy(next, state, len);
    next[len] = 0;
    bool changed = !e->hasState_ || strcmp(next, e->state_) != 0;
    strcpy(e->state_, next);
    e->hasState_ = true;
    if (changed && state_ == Active) sendStateOf(*e);
    return true;
}

bool Link::setNumber(const char* key, long value) {
    Entity* e = find(key);
    if (!e || !e->numeric_) return false;
    char text[sizeof(e->state_)];
    snprintf(text, sizeof(text), "%ld", value);
    bool changed = !e->hasState_ || strcmp(text, e->state_) != 0;
    strcpy(e->state_, text);
    e->hasState_ = true;
    if (changed && state_ == Active) sendStateOf(*e);
    return true;
}

bool Link::setNumber(const char* key, double value) {
    if (isnan(value) || isinf(value)) return false;
    Entity* e = find(key);
    if (!e || !e->numeric_) return false;
    char text[sizeof(e->state_)];
    snprintf(text, sizeof(text), "%.6g", value);
    bool changed = !e->hasState_ || strcmp(text, e->state_) != 0;
    strcpy(e->state_, text);
    e->hasState_ = true;
    if (changed && state_ == Active) sendStateOf(*e);
    return true;
}

bool Link::event(const char* entity, const char* name) {
    if (state_ != Active) return false;
    JsonWriter w(out_, sizeof(out_));
    begin_(w, "event");
    w.key("entity").str(entity).key("event").str(name).key("data").beginObject().endObject();
    return sendSigned(w);
}

bool Link::event(const char* entity, const char* name, EventData& data) {
    if (state_ != Active) return false;
    const char* json;
    size_t length;
    if (!data.finish(json, length)) return false;
    JsonWriter w(out_, sizeof(out_));
    begin_(w, "event");
    w.key("entity").str(entity).key("event").str(name).key("data").raw(json, length);
    return sendSigned(w);
}

void Link::onStatus(void (*handler)(void*, Status), void* context) {
    statusHandler_ = handler;
    statusContext_ = context;
}

void Link::setPairingButton(bool (*isPressed)(void*), void* context) {
    pairingButton_ = isPressed;
    pairingButtonContext_ = context;
}

void Link::confirmPairing() {
    if (state_ != Pending) return;
    JsonWriter w(out_, sizeof(out_));
    begin_(w, "pair_confirm");
    sendSigned(w);
}

void Link::factoryReset() {
    storage_.erase(SecretName);
    env_.random(secret_, sizeof(secret_));
    storage_.write(SecretName, secret_, sizeof(secret_));
    if (state_ != Idle && state_ != Backoff) dropConnection(false);
}

// --- connection management ---------------------------------------------------------------------------------------

void Link::setStatus(Status status) {
    if (status == status_) return;
    status_ = status;
    if (statusHandler_) statusHandler_(statusContext_, status);
}

uint32_t Link::jitter(uint32_t ms) {
    uint8_t r;
    env_.random(&r, 1);
    return ms + (ms / 4) * r / 255;  // up to +25 %: devices that lost the server together do not return together
}

void Link::scheduleRetry() {
    uint32_t now = env_.millis();
    state_ = Backoff;
    setStatus(StatusOffline);
    retryAt_ = now + jitter(backoffNow_);
    backoffNow_ = backoffNow_ * 2 > backoffMax_ ? backoffMax_ : backoffNow_ * 2;
    inLength_ = 0;
    inOverflow_ = false;
    pairingCode_[0] = 0;
}

void Link::startConnection() {
    uint32_t now = env_.millis();
    setStatus(StatusConnecting);
    // A bridge chip knows the server itself: it needs no address from the sketch.
    if ((!host_ && !transport_.autoConnects()) || !transport_.connect(host_ ? host_ : "", port_)) {
        transport_.stop();
        scheduleRetry();  // a server that is down is not asked every two seconds forever
        return;
    }
    state_ = AwaitHello;
    stateSince_ = lastReceived_ = now;
    inLength_ = 0;
    inOverflow_ = false;
    enrolling_ = false;
    outSeq_ = inSeq_ = 0;
}

void Link::dropConnection(bool serverFault) {
    (void)serverFault;
    transport_.stop();
    scheduleRetry();
}

void Link::loop() {
    if (!began_) return;
    uint32_t now = env_.millis();
    if (state_ == Idle || state_ == Backoff) {
        // A bridge chip connects on its own and the server expects the login right away: no waiting for the backoff.
        bool due = transport_.autoConnects() ? transport_.connected() : static_cast<int32_t>(now - retryAt_) >= 0;
        if (due) startConnection();
        return;
    }
    if (!transport_.connected()) {
        dropConnection(false);
        return;
    }
    pumpInput();
    if (state_ == Backoff) return;

    now = env_.millis();
    if ((state_ == AwaitHello || state_ == AwaitAuth) && now - stateSince_ > HandshakeTimeoutMs) {
        dropConnection(true);
    } else if (now - lastReceived_ > IdleTimeoutMs) {
        // Half-open connection (some TCP bridges do not notice): reset the transport.
        transport_.reset();
        dropConnection(false);
    } else if (state_ == Pending && pairingButton_) {
        bool pressed = pairingButton_(pairingButtonContext_);
        if (pressed && !buttonWasPressed_) confirmPairing();
        buttonWasPressed_ = pressed;
    } else if (state_ == Active && backoffNow_ != backoffMin_ && now - activeSince_ > StableAfterMs) {
        backoffNow_ = backoffMin_;
    }
}

void Link::pumpInput() {
    uint8_t chunk[64];
    size_t budget = 1024;  // keep the loop responsive even with a flood of input
    while (budget > 0 && state_ != Backoff && state_ != Idle && transport_.available() > 0) {
        size_t want = budget < sizeof(chunk) ? budget : sizeof(chunk);
        size_t n = transport_.read(chunk, want);
        if (n == 0) break;
        budget -= n;
        handleBytes(chunk, n);
    }
}

void Link::handleBytes(const uint8_t* data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (state_ == Backoff || state_ == Idle) return;
        char c = static_cast<char>(data[i]);
        if (c == '\n') {
            size_t n = inLength_;
            inLength_ = 0;
            if (n > 0 && in_[n - 1] == '\r') n--;
            if (n > 0) {
                in_[n] = 0;
                handleLine(in_, n);
            }
        } else if (inLength_ < sizeof(in_) - 1) {
            in_[inLength_++] = c;
        } else {
            // Longer than announced: the stream position is lost, start over with a new connection.
            dropConnection(true);
            return;
        }
    }
}

// --- incoming messages -------------------------------------------------------------------------------------------

bool Link::verifySigned(char* line, size_t length, JsonValue& document) {
    if (length < SignatureTrailer + 2) return false;
    size_t markerAt = length - SignatureTrailer;
    if (memcmp(line + markerAt, SigMarker, 8) != 0) return false;
    const char* hex = line + markerAt + 8;
    if (hex[32] != '"' || hex[33] != '}' || !isHex(hex, 32)) return false;
    uint8_t expected[16];
    if (!fromHex(hex, 32, expected, sizeof(expected))) return false;

    uint8_t mac[Sha256::DigestSize];
    sessionMac_.begin();
    sessionMac_.update(reinterpret_cast<const uint8_t*>(line), markerAt);
    sessionMac_.final(mac);
    if (!constantTimeEquals(mac, expected, sizeof(expected))) return false;

    line[markerAt] = '}';  // the signed text is everything before the comma, closed again
    return JsonValue::parse(line, markerAt + 1, document);
}

bool Link::acceptSequence(const JsonValue& document) {
    long seq;
    if (!document.get("seq").asInt(seq) || seq <= 0 || static_cast<uint32_t>(seq) <= inSeq_) return false;
    inSeq_ = static_cast<uint32_t>(seq);
    return true;
}

void Link::handleLine(char* line, size_t length) {
    uint32_t now = env_.millis();
    lastReceived_ = now;
    JsonValue document;
    switch (state_) {
        case AwaitHello:
            if (!JsonValue::parse(line, length, document)) return dropConnection(true);
            if (document.get("type").equals("hello")) return handleHello(document);
            if (document.get("type").equals("error")) return handleError(document);
            return dropConnection(true);
        case AwaitAuth:
            if (length > SignatureTrailer && memcmp(line + length - SignatureTrailer, SigMarker, 8) == 0) {
                if (!verifySigned(line, length, document) || !acceptSequence(document)) return dropConnection(true);
                return handleSigned(document, now);
            }
            if (!JsonValue::parse(line, length, document)) return dropConnection(true);
            if (document.get("type").equals("error")) return handleError(document);
            return dropConnection(true);
        case Pending:
        case Active:
            // From here on only authentic messages count: anything else ends the connection.
            if (!verifySigned(line, length, document) || !acceptSequence(document)) return dropConnection(true);
            return handleSigned(document, now);
        default:
            return;
    }
}

void Link::handleHello(const JsonValue& document) {
    long proto;
    if (!document.get("proto").asInt(proto) || proto != AMLINK_PROTOCOL) return dropConnection(true);
    if (!document.get("nonce").asString(serverNonce_, sizeof(serverNonce_)) || strlen(serverNonce_) < 16) return dropConnection(true);
    sendAuth(serverNonce_);
}

void Link::handleError(const JsonValue& document) {
    if (document.get("code").equals("unknown_device") && state_ == AwaitAuth && !enrolling_) {
        // First contact with this server: introduce ourselves once; the server then asks to log in again.
        enrolling_ = true;
        sendEnroll();
        if (state_ != Backoff) {
            state_ = AwaitHello;
            stateSince_ = env_.millis();
        }
        return;
    }
    dropConnection(true);
}

void Link::handleSigned(const JsonValue& document, uint32_t receivedAt) {
    char type[24];
    if (!document.get("type").asString(type, sizeof(type))) return;

    if (strcmp(type, "welcome") == 0) {
        if (state_ == AwaitAuth || state_ == Pending) enterActive();
    } else if (strcmp(type, "pending") == 0) {
        if (state_ == AwaitAuth || state_ == Pending) enterPending(document);
    } else if (strcmp(type, "ping") == 0) {
        JsonWriter w(out_, sizeof(out_));
        begin_(w, "pong");
        sendSigned(w);
    } else if (strcmp(type, "call") == 0) {
        if (state_ == Active) handleCall(document, receivedAt);
    } else if (strcmp(type, "error") == 0) {
        // e.g. pairing_closed: nothing to do, the server decides about the connection
    } else {
        JsonWriter w(out_, sizeof(out_));
        begin_(w, "error");
        w.key("code").str("unsupported").key("ref").str(type);
        sendSigned(w);
    }
}

void Link::computePairingCode() {
    uint8_t mac[Sha256::DigestSize];
    Hmac::compute(secret_, sizeof(secret_), reinterpret_cast<const uint8_t*>("pair"), 4, mac);
    uint32_t n = (uint32_t(mac[0]) << 24) | (uint32_t(mac[1]) << 16) | (uint32_t(mac[2]) << 8) | mac[3];
    snprintf(pairingCode_, sizeof(pairingCode_), "%06lu", static_cast<unsigned long>(n % 1000000UL));
}

void Link::enterPending(const JsonValue& document) {
    (void)document;
    bool first = state_ != Pending;
    state_ = Pending;
    computePairingCode();  // computed from our own secret: it only matches the server's if both know the same one
    buttonWasPressed_ = pairingButton_ ? pairingButton_(pairingButtonContext_) : false;
    setStatus(StatusPairing);
    if (first) {
        // The administrator sees what the device offers while deciding about the approval.
        announceEntities();
        announceStates();
    }
}

void Link::enterActive() {
    state_ = Active;
    activeSince_ = env_.millis();
    pairingCode_[0] = 0;
    setStatus(StatusOnline);
    announceEntities();
    announceStates();
}

void Link::handleCall(const JsonValue& document, uint32_t receivedAt) {
    Call call;
    if (!document.get("id").asString(call.id_, sizeof(call.id_)) || call.id_[0] == 0) return;  // cannot answer without an id
    if (!document.get("entity").asString(call.entity_, sizeof(call.entity_)) ||
        !document.get("action").asString(call.action_, sizeof(call.action_))) {
        return sendResult(call.id_, AM_INVALID, "invalid", nullptr);
    }
    call.data_ = document.get("data");
    long ttl = 5000;
    document.get("ttl").asInt(ttl);
    call.ttl_ = ttl < 0 ? 0 : (ttl > 60000 ? 60000 : static_cast<uint32_t>(ttl));

    Entity* target = find(call.entity_);
    if (!target) return sendResult(call.id_, AM_FAILED, "unknown_entity", nullptr);
    if (!target->handler_) return sendResult(call.id_, AM_UNSUPPORTED, "unsupported", nullptr);
    // A call must not run later than the server wanted (a door must not open seconds after the request).
    if (env_.millis() - receivedAt > call.ttl_) return sendResult(call.id_, AM_FAILED, "expired", nullptr);

    int code = target->handler_(target->context_, call);
    sendResult(call.id_, code, resultText(code), call.message_);
}

// --- outgoing messages -------------------------------------------------------------------------------------------

bool Link::begin_(JsonWriter& w, const char* type) {
    w.beginObject().key("type").str(type);
    return w.ok();
}

bool Link::writeAll(const uint8_t* data, size_t length) {
    uint32_t start = env_.millis();
    while (length > 0) {
        size_t n = transport_.write(data, length);
        data += n;
        length -= n;
        if (n == 0 && env_.millis() - start > WriteTimeoutMs) {
            dropConnection(false);
            return false;
        }
    }
    return true;
}

bool Link::sendPlain(JsonWriter& w) {
    w.endObject();
    w.append("\n", 1);
    if (!w.ok()) return false;
    return writeAll(reinterpret_cast<const uint8_t*>(w.data()), w.size());
}

bool Link::sendSigned(JsonWriter& w) {
    w.key("seq").num(static_cast<long>(outSeq_ + 1));
    if (!w.ok()) return false;
    outSeq_++;
    uint8_t mac[Sha256::DigestSize];
    char hex[2 * Sha256::DigestSize + 1];
    sessionMac_.begin();
    sessionMac_.update(reinterpret_cast<const uint8_t*>(w.data()), w.size());
    sessionMac_.final(mac);
    toHex(mac, 16, hex);
    w.append(SigMarker, 8).append(hex, 32).append("\"}\n", 3);
    if (!w.ok()) return false;
    return writeAll(reinterpret_cast<const uint8_t*>(w.data()), w.size());
}

void Link::sendAuth(const char* serverNonce) {
    uint8_t nonce[16];
    env_.random(nonce, sizeof(nonce));
    toHex(nonce, sizeof(nonce), deviceNonce_);

    uint8_t mac[Sha256::DigestSize];
    char macHex[2 * Sha256::DigestSize + 1];
    Hmac auth;
    auth.init(secret_, sizeof(secret_));
    auth.update("auth|"); auth.update(serverNonce); auth.update("|"); auth.update(deviceNonce_); auth.update("|"); auth.update(deviceId_);
    auth.final(mac);
    toHex(mac, sizeof(mac), macHex);

    uint8_t key[Sha256::DigestSize];
    Hmac session;
    session.init(secret_, sizeof(secret_));
    session.update("session|"); session.update(serverNonce); session.update("|"); session.update(deviceNonce_);
    session.final(key);
    sessionMac_.init(key, sizeof(key));
    memset(key, 0, sizeof(key));
    outSeq_ = inSeq_ = 0;

    JsonWriter w(out_, sizeof(out_));
    begin_(w, "auth");
    w.key("proto").num(static_cast<long>(AMLINK_PROTOCOL)).key("device").str(deviceId_).key("nonce").str(deviceNonce_).key("mac").str(macHex);
    w.key("model").str(identity_.model).key("fw").str(identity_.firmware).key("platform").str(env_.platform());
    w.key("lib").str(AMLINK_VERSION).key("transport").str(transport_.kind()).key("max_line").num(static_cast<long>(AMLINK_LINE_SIZE));
    if (sendPlain(w)) {
        state_ = AwaitAuth;
        stateSince_ = env_.millis();
    }
}

void Link::sendEnroll() {
    char secretHex[2 * sizeof(secret_) + 1];
    toHex(secret_, sizeof(secret_), secretHex);
    JsonWriter w(out_, sizeof(out_));
    begin_(w, "enroll");
    w.key("device").str(deviceId_).key("secret").str(secretHex).key("model").str(identity_.model).key("fw").str(identity_.firmware);
    w.key("platform").str(env_.platform()).key("lib").str(AMLINK_VERSION).key("transport").str(transport_.kind());
    sendPlain(w);
    memset(secretHex, 0, sizeof(secretHex));
}

void Link::sendResult(const char* id, int code, const char* error, const char* message) {
    JsonWriter w(out_, sizeof(out_));
    begin_(w, "result");
    w.key("id").str(id).key("ok").boolean(code == AM_OK);
    if (code != AM_OK) w.key("error").str(error);
    if (message && message[0]) w.key("message").str(message);
    sendSigned(w);
}

void Link::writeEntity(JsonWriter& w, const Entity& e) {
    w.beginObject().key("key").str(e.key_).key("domain").str(e.domain_);
    if (e.label_) w.key("name").str(e.label_);
    if (e.class_) w.key("class").str(e.class_);
    if (e.category_) w.key("category").str(e.category_);
    if (e.featureCount_ > 0) {
        w.key("features").beginArray();
        for (uint8_t i = 0; i < e.featureCount_; i++) w.str(e.features_[i]);
        w.endArray();
    }
    if (e.attrCount_ > 0) {
        w.key("attrs").beginObject();
        for (uint8_t i = 0; i < e.attrCount_; i++) {
            const Entity::Attr& a = e.attrs_[i];
            w.key(a.name);
            switch (a.kind) {
                case Entity::AttrInt: w.num(a.number); break;
                case Entity::AttrString: w.str(a.text); break;
                case Entity::AttrBool: w.boolean(a.number != 0); break;
                case Entity::AttrOptions:
                    w.beginArray();
                    for (long k = 0; k < a.number; k++) w.str(a.list[k]);
                    w.endArray();
                    break;
                default: w.null();
            }
        }
        w.endObject();
    }
    w.endObject();
}

void Link::writeState(JsonWriter& w, const Entity& e) {
    w.beginObject().key("entity").str(e.key_).key("state");
    if (e.numeric_) w.raw(e.state_, strlen(e.state_)); else w.str(e.state_);
    w.endObject();
}

// Announces all entities. A device with little memory cannot fit them in one line: the parts are flagged with
// "more":true and the server evaluates the announcement when the last part arrived.
void Link::announceEntities() {
    const size_t budget = sizeof(out_) - MessageOverhead - 16;  // 16: ,"more":true
    uint8_t next = 0;
    do {
        JsonWriter w(out_, sizeof(out_));
        begin_(w, "entities");
        w.key("entities").beginArray();
        size_t added = 0;
        for (; next < entityCount_; next++) {
            char scratch[384];
            JsonWriter one(scratch, sizeof(scratch));
            writeEntity(one, entities_[next]);
            if (!one.ok()) continue;  // a single entity larger than the scratch buffer is skipped
            if (w.size() + one.size() + 2 > budget) {
                if (added == 0) continue;  // does not fit even alone
                break;
            }
            w.raw(scratch, one.size());
            added++;
        }
        w.endArray();
        if (next < entityCount_) w.key("more").boolean(true);
        if (!sendSigned(w) || state_ == Backoff) return;
    } while (next < entityCount_);
}

void Link::announceStates() {
    const size_t budget = sizeof(out_) - MessageOverhead;
    uint8_t next = 0;
    while (next < entityCount_) {
        JsonWriter w(out_, sizeof(out_));
        begin_(w, "states");
        w.key("states").beginArray();
        size_t added = 0;
        for (; next < entityCount_; next++) {
            if (!hasState(entities_[next])) continue;
            char scratch[160];
            JsonWriter one(scratch, sizeof(scratch));
            writeState(one, entities_[next]);
            if (!one.ok()) continue;
            if (w.size() + one.size() + 2 > budget) break;
            w.raw(scratch, one.size());
            added++;
        }
        w.endArray();
        if (added == 0) return;
        if (!sendSigned(w) || state_ == Backoff) return;
    }
}

bool Link::sendStateOf(Entity& e) {
    if (!hasState(e)) return false;
    JsonWriter w(out_, sizeof(out_));
    begin_(w, "state");
    w.key("entity").str(e.key_).key("state");
    if (e.numeric_) w.raw(e.state_, strlen(e.state_)); else w.str(e.state_);
    return sendSigned(w);
}

}  // namespace amlink
