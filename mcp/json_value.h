#ifndef AXON_MCP_JSON_VALUE_H
#define AXON_MCP_JSON_VALUE_H

#include <string>
#include <vector>
#include <utility>
#include <cstddef>

/*
 * Minimal JSON value type for the Axon MCP server.
 *
 * It both parses a UTF-8 JSON document into a value tree and serializes a value
 * tree back to a compact JSON string, so the same module handles incoming
 * requests and outgoing responses. No external dependencies.
 *
 * Objects preserve insertion order, which keeps serialized output stable and
 * readable (and test assertions deterministic). Lookups are linear, which is
 * fine for the small messages the protocol exchanges.
 */

enum class JsonType { Null, Bool, Number, String, Array, Object };

class JsonValue {
public:
    JsonValue() : type_(JsonType::Null) {}

    /* --- builders --- */
    static JsonValue null() { return JsonValue(); }
    static JsonValue boolean(bool b);
    static JsonValue number(double n);
    static JsonValue integer(long n);
    static JsonValue string(std::string s);
    static JsonValue array();
    static JsonValue object();

    /* --- type inspection --- */
    JsonType type() const { return type_; }
    bool is_null() const { return type_ == JsonType::Null; }
    bool is_bool() const { return type_ == JsonType::Bool; }
    bool is_number() const { return type_ == JsonType::Number; }
    bool is_string() const { return type_ == JsonType::String; }
    bool is_array() const { return type_ == JsonType::Array; }
    bool is_object() const { return type_ == JsonType::Object; }

    /* --- typed accessors (return the default on a type mismatch) --- */
    bool get_bool(bool def = false) const;
    double get_double(double def = 0.0) const;
    long get_int(long def = 0) const;
    std::string get_string(const std::string &def = "") const;

    /* Object member by key, or nullptr if this is not an object or the key is
     * absent. */
    const JsonValue *get_object_member(const std::string &key) const;

    /* Array elements (empty if this is not an array). */
    const std::vector<JsonValue> &get_array() const;

    /* Ordered object members (empty if this is not an object). */
    const std::vector<std::pair<std::string, JsonValue>> &object_items() const;

    /* Element count for arrays and objects; 0 otherwise. */
    size_t size() const;

    /* --- mutation (for building output) --- */
    void set(const std::string &key, JsonValue value);  /* object */
    void push_back(JsonValue value);                    /* array */

    /* --- serialization --- */
    std::string dump() const;

    /* --- parsing --- */
    /* Parse input into a value. On failure returns a Null value and sets err to
     * a message; on success err is left empty. */
    static JsonValue parse(const std::string &input, std::string &err);

private:
    JsonType type_;
    bool bool_ = false;
    double num_ = 0.0;
    std::string str_;
    std::vector<JsonValue> arr_;
    std::vector<std::pair<std::string, JsonValue>> obj_;
};

#endif
