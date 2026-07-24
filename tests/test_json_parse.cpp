#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <string>
#include "../mcp/json_value.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    printf("  test_%s: ", #name); \
    test_##name(); \
    tests_passed++; \
    printf("PASS\n"); \
} while(0)

static JsonValue ok_parse(const std::string &in)
{
    std::string err;
    JsonValue v = JsonValue::parse(in, err);
    assert(err.empty());
    return v;
}

static void expect_error(const std::string &in)
{
    std::string err;
    JsonValue v = JsonValue::parse(in, err);
    assert(!err.empty());        /* an error message was produced */
    assert(v.is_null());         /* and the returned value is Null */
}

/* --- valid documents --- */

void test_parse_object()
{
    tests_run++;
    JsonValue v = ok_parse("{\"name\":\"axon\",\"n\":42,\"ok\":true,\"x\":null}");
    assert(v.is_object());
    assert(v.size() == 4);
    const JsonValue *name = v.get_object_member("name");
    assert(name && name->is_string() && name->get_string() == "axon");
    const JsonValue *n = v.get_object_member("n");
    assert(n && n->is_number() && n->get_int() == 42);
    assert(v.get_object_member("ok")->get_bool() == true);
    assert(v.get_object_member("x")->is_null());
    assert(v.get_object_member("missing") == nullptr);
}

void test_parse_array_and_nesting()
{
    tests_run++;
    JsonValue v = ok_parse("[1, [2, [3, {\"k\":[4]}]], 5]");
    assert(v.is_array());
    assert(v.size() == 3);
    assert(v.get_array()[0].get_int() == 1);
    const JsonValue &inner = v.get_array()[1];
    assert(inner.is_array() && inner.get_array()[0].get_int() == 2);
}

void test_parse_numbers()
{
    tests_run++;
    assert(ok_parse("0").get_int() == 0);
    assert(ok_parse("-17").get_int() == -17);
    assert(ok_parse("3.5").get_double() == 3.5);
    assert(ok_parse("1e3").get_double() == 1000.0);
    assert(ok_parse("-2.5e-1").get_double() == -0.25);
}

void test_parse_literals()
{
    tests_run++;
    assert(ok_parse("true").get_bool() == true);
    assert(ok_parse("false").get_bool() == false);
    assert(ok_parse("null").is_null());
    assert(ok_parse("  \n\t \"hi\" ").get_string() == "hi");  /* surrounding ws ok */
}

void test_string_escapes()
{
    tests_run++;
    assert(ok_parse("\"a\\nb\\tc\"").get_string() == "a\nb\tc");
    assert(ok_parse("\"quote:\\\" slash:\\\\ fwd:\\/\"").get_string()
           == "quote:\" slash:\\ fwd:/");
    assert(ok_parse("\"\\b\\f\\r\"").get_string() == "\b\f\r");
    assert(ok_parse("\"\\u0041\\u0042\"").get_string() == "AB");
    /* Surrogate pair for U+1F600 -> 4-byte UTF-8 F0 9F 98 80 */
    std::string emoji = ok_parse("\"\\uD83D\\uDE00\"").get_string();
    assert(emoji.size() == 4);
    assert((unsigned char)emoji[0] == 0xF0 && (unsigned char)emoji[1] == 0x9F &&
           (unsigned char)emoji[2] == 0x98 && (unsigned char)emoji[3] == 0x80);
}

void test_deep_nesting_ok()
{
    tests_run++;
    std::string in = std::string(64, '[') + std::string(64, ']');  /* exactly 64 */
    std::string err;
    JsonValue v = JsonValue::parse(in, err);
    assert(err.empty());
    assert(v.is_array());
}

/* --- malformed inputs each fail cleanly (no crash) --- */

void test_malformed_inputs()
{
    tests_run++;
    expect_error("");                       /* empty */
    expect_error("   ");                     /* whitespace only */
    expect_error("{\"a\":1");                /* truncated object */
    expect_error("[1,2");                    /* truncated array */
    expect_error("{\"a\":1} garbage");       /* trailing garbage */
    expect_error("\"unterminated");          /* unterminated string */
    expect_error("\"bad\\x escape\"");       /* invalid escape */
    expect_error("\"\\uZZZZ\"");             /* bad \u */
    expect_error("\"\\uD83D\"");             /* lone high surrogate */
    expect_error("01");                       /* leading zero */
    expect_error("+1");                       /* leading plus */
    expect_error("tru");                      /* partial literal */
    std::string too_deep = std::string(65, '[') + std::string(65, ']');
    expect_error(too_deep);                   /* exceeds depth limit */
}

/* --- serialization + round-trip --- */

void test_dump_basic()
{
    tests_run++;
    JsonValue obj = JsonValue::object();
    obj.set("name", JsonValue::string("axon"));
    obj.set("limit", JsonValue::integer(20));
    obj.set("ok", JsonValue::boolean(true));
    JsonValue arr = JsonValue::array();
    arr.push_back(JsonValue::integer(1));
    arr.push_back(JsonValue::string("two"));
    obj.set("items", std::move(arr));
    /* Insertion order preserved; integers print without a fractional part. */
    assert(obj.dump() == "{\"name\":\"axon\",\"limit\":20,\"ok\":true,\"items\":[1,\"two\"]}");
}

void test_dump_escapes()
{
    tests_run++;
    JsonValue s = JsonValue::string("line1\nline2\t\"q\"\\");
    assert(s.dump() == "\"line1\\nline2\\t\\\"q\\\"\\\\\"");
}

void test_round_trip()
{
    tests_run++;
    const char *doc = "{\"a\":[1,2.5,true,null,\"x\\ny\"],\"b\":{\"c\":-3}}";
    JsonValue v1 = ok_parse(doc);
    std::string d1 = v1.dump();
    JsonValue v2 = ok_parse(d1);
    std::string d2 = v2.dump();
    assert(d1 == d2);           /* dump is stable across a re-parse */
}

int main()
{
    printf("=== JSON Parse Tests ===\n");

    TEST(parse_object);
    TEST(parse_array_and_nesting);
    TEST(parse_numbers);
    TEST(parse_literals);
    TEST(string_escapes);
    TEST(deep_nesting_ok);
    TEST(malformed_inputs);
    TEST(dump_basic);
    TEST(dump_escapes);
    TEST(round_trip);

    printf("\n-----------------------\n");
    printf("%d Tests %d Failures 0 Ignored\n", tests_passed, tests_run - tests_passed);
    printf("OK\n");

    return 0;
}
