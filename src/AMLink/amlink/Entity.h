#pragma once
#include "Config.h"
#include "Json.h"

namespace amlink {

/** Result codes returned by an action handler. */
enum Result {
    AM_OK = 0,
    AM_FAILED = 1,       // tried and failed
    AM_UNSUPPORTED = 2,  // action not available
    AM_BUSY = 3,         // cannot do it right now
    AM_INVALID = 4       // arguments are wrong
};

class Link;

/** A call from the server (e.g. "unlock" on a lock entity), valid only inside the handler. */
class Call {
public:
    const char* id() const { return id_; }
    const char* entity() const { return entity_; }
    const char* action() const { return action_; }
    bool is(const char* action) const;
    /** Milliseconds after which the server no longer wants this call executed (already checked by the library). */
    uint32_t ttl() const { return ttl_; }

    /** Argument from the call data, `fallback` if missing or of the wrong type. */
    long intArg(const char* name, long fallback) const;
    double numberArg(const char* name, double fallback) const;
    bool boolArg(const char* name, bool fallback) const;
    /** Decoded UTF-8 text argument; false if missing or longer than `capacity`-1. */
    bool stringArg(const char* name, char* out, size_t capacity) const;
    /** The raw data object, e.g. to read an array of display lines. */
    const JsonValue& data() const { return data_; }

    /** Optional short text that is sent back with the result (shown in the server's history). */
    void message(const char* text);

private:
    friend class Link;
    Call() : ttl_(0), data_(), message_() { id_[0] = entity_[0] = action_[0] = 0; message_[0] = 0; }
    char id_[65];
    char entity_[49];
    char action_[33];
    uint32_t ttl_;
    JsonValue data_;
    char message_[61];
};

typedef int (*ActionFn)(void* context, Call& call);

/**
 * One addressable unit of a device: a lock, a reader, a sensor, a display, ...
 * Created with Link::entity(); the descriptive strings are NOT copied, pass string literals.
 */
class Entity {
public:
    /** Proposed display name (administrators can rename it in the server). */
    Entity& label(const char* text) { label_ = text; return *this; }
    /** Finer kind within the domain, e.g. "door" for a binary_sensor. */
    Entity& cls(const char* text) { class_ = text; return *this; }
    /** "config" for settings the server keeps a target value for, "diagnostic" for information only. */
    Entity& category(const char* text) { category_ = text; return *this; }
    Entity& feature(const char* name);
    Entity& attr(const char* name, long value);
    Entity& attr(const char* name, const char* value);
    Entity& attrBool(const char* name, bool value);
    /** Allowed values of a "select" entity. */
    Entity& options(const char* const* list, uint8_t count);
    /** Handler for calls from the server. Runs inside Link::loop(). */
    Entity& onAction(ActionFn handler, void* context = nullptr) { handler_ = handler; context_ = context; return *this; }

    const char* key() const { return key_; }
    const char* domain() const { return domain_; }
    /** Current state as last set ("" if none yet). */
    const char* state() const { return state_; }

private:
    friend class Link;
    Entity() { clear(); }
    void clear();

    enum AttrKind { AttrNone, AttrInt, AttrString, AttrBool, AttrOptions };
    struct Attr {
        const char* name;
        AttrKind kind;
        long number;
        const char* text;
        const char* const* list;
    };

    const char* key_;
    const char* domain_;
    const char* label_;
    const char* class_;
    const char* category_;
    const char* features_[AMLINK_MAX_FEATURES];
    uint8_t featureCount_;
    Attr attrs_[AMLINK_MAX_ATTRS];
    uint8_t attrCount_;
    ActionFn handler_;
    void* context_;
    char state_[28];
    bool hasState_;
    bool numeric_;  // domain "number": the state is sent as a JSON number
};

/** Data of an event, e.g. EventData d; d.str("method", "rfid").str("value", "04a1b2c3"); */
class EventData {
public:
    EventData() : writer_(buffer_, sizeof(buffer_)), closed_(false) { writer_.beginObject(); }
    EventData& str(const char* key, const char* value) { writer_.key(key).str(value); return *this; }
    EventData& num(const char* key, long value) { writer_.key(key).num(value); return *this; }
    EventData& boolean(const char* key, bool value) { writer_.key(key).boolean(value); return *this; }

private:
    friend class Link;
    EventData(const EventData&);             // not copyable: the writer points into this object
    EventData& operator=(const EventData&);
    /** Closes the object once and returns the JSON text; false if the data did not fit. */
    bool finish(const char*& json, size_t& length) {
        if (!closed_) {
            writer_.endObject();
            closed_ = true;
        }
        json = writer_.data();
        length = writer_.size();
        return writer_.ok();
    }
    char buffer_[192];
    JsonWriter writer_;
    bool closed_;
};

}  // namespace amlink
