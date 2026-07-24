#include "json_value.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>

/* ---------- builders ---------- */

JsonValue JsonValue::boolean(bool b)
{
    JsonValue v;
    v.type_ = JsonType::Bool;
    v.bool_ = b;
    return v;
}

JsonValue JsonValue::number(double n)
{
    JsonValue v;
    v.type_ = JsonType::Number;
    v.num_ = n;
    return v;
}

JsonValue JsonValue::integer(long n)
{
    return number(static_cast<double>(n));
}

JsonValue JsonValue::string(std::string s)
{
    JsonValue v;
    v.type_ = JsonType::String;
    v.str_ = std::move(s);
    return v;
}

JsonValue JsonValue::array()
{
    JsonValue v;
    v.type_ = JsonType::Array;
    return v;
}

JsonValue JsonValue::object()
{
    JsonValue v;
    v.type_ = JsonType::Object;
    return v;
}

/* ---------- accessors ---------- */

bool JsonValue::get_bool(bool def) const
{
    return type_ == JsonType::Bool ? bool_ : def;
}

double JsonValue::get_double(double def) const
{
    return type_ == JsonType::Number ? num_ : def;
}

long JsonValue::get_int(long def) const
{
    return type_ == JsonType::Number ? static_cast<long>(num_) : def;
}

std::string JsonValue::get_string(const std::string &def) const
{
    return type_ == JsonType::String ? str_ : def;
}

const JsonValue *JsonValue::get_object_member(const std::string &key) const
{
    if (type_ != JsonType::Object)
        return nullptr;
    for (const auto &kv : obj_) {
        if (kv.first == key)
            return &kv.second;
    }
    return nullptr;
}

const std::vector<JsonValue> &JsonValue::get_array() const
{
    static const std::vector<JsonValue> empty;
    return type_ == JsonType::Array ? arr_ : empty;
}

const std::vector<std::pair<std::string, JsonValue>> &JsonValue::object_items() const
{
    static const std::vector<std::pair<std::string, JsonValue>> empty;
    return type_ == JsonType::Object ? obj_ : empty;
}

size_t JsonValue::size() const
{
    if (type_ == JsonType::Array)
        return arr_.size();
    if (type_ == JsonType::Object)
        return obj_.size();
    return 0;
}

void JsonValue::set(const std::string &key, JsonValue value)
{
    type_ = JsonType::Object;
    for (auto &kv : obj_) {
        if (kv.first == key) {   /* replace existing key, keep position */
            kv.second = std::move(value);
            return;
        }
    }
    obj_.emplace_back(key, std::move(value));
}

void JsonValue::push_back(JsonValue value)
{
    type_ = JsonType::Array;
    arr_.push_back(std::move(value));
}

/* ---------- serialization ---------- */

static void dump_string(const std::string &s, std::string &out)
{
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b";  break;
        case '\f': out += "\\f";  break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (c < 0x20) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            } else {
                out += static_cast<char>(c);
            }
            break;
        }
    }
    out += '"';
}

static void dump_number(double n, std::string &out)
{
    char buf[32];
    /* Print integral values without a fractional part; otherwise a
     * round-trippable double. */
    if (std::isfinite(n) && n >= -9.007199254740992e15 &&
        n <= 9.007199254740992e15 &&
        n == static_cast<double>(static_cast<long long>(n))) {
        snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(n));
    } else {
        snprintf(buf, sizeof(buf), "%.17g", n);
    }
    out += buf;
}

static void dump_value(const JsonValue &v, std::string &out);

static void dump_value(const JsonValue &v, std::string &out)
{
    switch (v.type()) {
    case JsonType::Null:
        out += "null";
        break;
    case JsonType::Bool:
        out += v.get_bool() ? "true" : "false";
        break;
    case JsonType::Number:
        dump_number(v.get_double(), out);
        break;
    case JsonType::String:
        dump_string(v.get_string(), out);
        break;
    case JsonType::Array: {
        out += '[';
        const auto &items = v.get_array();
        for (size_t i = 0; i < items.size(); i++) {
            if (i > 0)
                out += ',';
            dump_value(items[i], out);
        }
        out += ']';
        break;
    }
    case JsonType::Object: {
        out += '{';
        const auto &members = v.object_items();
        for (size_t i = 0; i < members.size(); i++) {
            if (i > 0)
                out += ',';
            dump_string(members[i].first, out);
            out += ':';
            dump_value(members[i].second, out);
        }
        out += '}';
        break;
    }
    }
}

std::string JsonValue::dump() const
{
    std::string out;
    dump_value(*this, out);
    return out;
}

/* ---------- parsing ---------- */

namespace {

const int MAX_DEPTH = 64;

struct Parser {
    const std::string &s;
    size_t i;
    std::string &err;
    bool failed;

    Parser(const std::string &str, std::string &e)
        : s(str), i(0), err(e), failed(false) {}

    void fail(const char *msg)
    {
        if (!failed) {
            err = msg;
            failed = true;
        }
    }

    bool eof() const { return i >= s.size(); }
    char peek() const { return i < s.size() ? s[i] : '\0'; }

    void skip_ws()
    {
        while (i < s.size()) {
            char c = s[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
                i++;
            else
                break;
        }
    }

    bool literal(const char *lit)
    {
        size_t n = std::strlen(lit);
        if (s.compare(i, n, lit) == 0) {
            i += n;
            return true;
        }
        return false;
    }

    bool hex4(unsigned &out)
    {
        if (i + 4 > s.size())
            return false;
        unsigned val = 0;
        for (int k = 0; k < 4; k++) {
            char c = s[i + k];
            val <<= 4;
            if (c >= '0' && c <= '9')      val |= (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') val |= (unsigned)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') val |= (unsigned)(c - 'A' + 10);
            else return false;
        }
        i += 4;
        out = val;
        return true;
    }

    static void encode_utf8(unsigned cp, std::string &out)
    {
        if (cp <= 0x7F) {
            out += static_cast<char>(cp);
        } else if (cp <= 0x7FF) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    /* Parse a JSON string body; assumes the opening quote is at s[i]. */
    bool raw_string(std::string &out)
    {
        if (peek() != '"') {
            fail("expected string");
            return false;
        }
        i++;  /* opening quote */
        while (!eof()) {
            char c = s[i++];
            if (c == '"')
                return true;
            if (c == '\\') {
                if (eof()) {
                    fail("unterminated escape");
                    return false;
                }
                char e = s[i++];
                switch (e) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case '/':  out += '/';  break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'u': {
                    unsigned cp;
                    if (!hex4(cp)) {
                        fail("invalid \\u escape");
                        return false;
                    }
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        /* high surrogate — must be followed by a low one */
                        if (i + 1 >= s.size() || s[i] != '\\' || s[i + 1] != 'u') {
                            fail("unpaired surrogate");
                            return false;
                        }
                        i += 2;
                        unsigned lo;
                        if (!hex4(lo) || lo < 0xDC00 || lo > 0xDFFF) {
                            fail("invalid low surrogate");
                            return false;
                        }
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        fail("unexpected low surrogate");
                        return false;
                    }
                    encode_utf8(cp, out);
                    break;
                }
                default:
                    fail("invalid escape");
                    return false;
                }
            } else if (static_cast<unsigned char>(c) < 0x20) {
                fail("control character in string");
                return false;
            } else {
                out += c;
            }
        }
        fail("unterminated string");
        return false;
    }

    JsonValue parse_number()
    {
        size_t start = i;
        if (peek() == '-')
            i++;
        if (peek() == '0') {
            i++;
        } else if (peek() >= '1' && peek() <= '9') {
            while (peek() >= '0' && peek() <= '9')
                i++;
        } else {
            fail("invalid number");
            return JsonValue();
        }
        if (peek() == '.') {
            i++;
            if (!(peek() >= '0' && peek() <= '9')) {
                fail("invalid fraction");
                return JsonValue();
            }
            while (peek() >= '0' && peek() <= '9')
                i++;
        }
        if (peek() == 'e' || peek() == 'E') {
            i++;
            if (peek() == '+' || peek() == '-')
                i++;
            if (!(peek() >= '0' && peek() <= '9')) {
                fail("invalid exponent");
                return JsonValue();
            }
            while (peek() >= '0' && peek() <= '9')
                i++;
        }
        std::string tok = s.substr(start, i - start);
        return JsonValue::number(std::strtod(tok.c_str(), nullptr));
    }

    JsonValue parse_value(int depth)
    {
        skip_ws();
        if (eof()) {
            fail("unexpected end of input");
            return JsonValue();
        }
        char c = peek();
        switch (c) {
        case '{': return parse_object(depth);
        case '[': return parse_array(depth);
        case '"': {
            std::string str;
            if (!raw_string(str))
                return JsonValue();
            return JsonValue::string(std::move(str));
        }
        case 't':
            if (literal("true")) return JsonValue::boolean(true);
            fail("invalid literal");
            return JsonValue();
        case 'f':
            if (literal("false")) return JsonValue::boolean(false);
            fail("invalid literal");
            return JsonValue();
        case 'n':
            if (literal("null")) return JsonValue::null();
            fail("invalid literal");
            return JsonValue();
        default:
            if (c == '-' || (c >= '0' && c <= '9'))
                return parse_number();
            fail("unexpected character");
            return JsonValue();
        }
    }

    JsonValue parse_object(int depth)
    {
        if (depth + 1 > MAX_DEPTH) {
            fail("nesting too deep");
            return JsonValue();
        }
        i++;  /* '{' */
        JsonValue obj = JsonValue::object();
        skip_ws();
        if (peek() == '}') {
            i++;
            return obj;
        }
        while (true) {
            skip_ws();
            std::string key;
            if (!raw_string(key))
                return JsonValue();
            skip_ws();
            if (peek() != ':') {
                fail("expected ':'");
                return JsonValue();
            }
            i++;
            JsonValue val = parse_value(depth + 1);
            if (failed)
                return JsonValue();
            obj.set(key, std::move(val));
            skip_ws();
            char c = peek();
            if (c == ',') {
                i++;
                continue;
            }
            if (c == '}') {
                i++;
                return obj;
            }
            fail("expected ',' or '}'");
            return JsonValue();
        }
    }

    JsonValue parse_array(int depth)
    {
        if (depth + 1 > MAX_DEPTH) {
            fail("nesting too deep");
            return JsonValue();
        }
        i++;  /* '[' */
        JsonValue arr = JsonValue::array();
        skip_ws();
        if (peek() == ']') {
            i++;
            return arr;
        }
        while (true) {
            JsonValue val = parse_value(depth + 1);
            if (failed)
                return JsonValue();
            arr.push_back(std::move(val));
            skip_ws();
            char c = peek();
            if (c == ',') {
                i++;
                continue;
            }
            if (c == ']') {
                i++;
                return arr;
            }
            fail("expected ',' or ']'");
            return JsonValue();
        }
    }
};

}  /* namespace */

JsonValue JsonValue::parse(const std::string &input, std::string &err)
{
    err.clear();
    Parser p(input, err);
    p.skip_ws();
    if (p.eof()) {
        err = "empty input";
        return JsonValue();
    }
    JsonValue v = p.parse_value(0);
    if (p.failed)
        return JsonValue();
    p.skip_ws();
    if (!p.eof()) {
        err = "trailing characters after JSON value";
        return JsonValue();
    }
    return v;
}
