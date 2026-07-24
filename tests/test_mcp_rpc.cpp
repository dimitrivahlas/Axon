#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <string>
#include "../mcp/rpc.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    printf("  test_%s: ", #name); \
    test_##name(); \
    tests_passed++; \
    printf("PASS\n"); \
} while(0)

/* substring check */
static bool has(const std::string &s, const char *sub)
{
    return s.find(sub) != std::string::npos;
}

void test_initialize_shape()
{
    tests_run++;
    std::string r = rpc_handle_message(
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\","
        "\"params\":{\"protocolVersion\":\"2025-06-18\"}}");
    assert(has(r, "\"jsonrpc\":\"2.0\""));
    assert(has(r, "\"id\":1"));
    assert(has(r, "\"result\""));
    assert(has(r, "\"protocolVersion\":\"2025-06-18\""));
    assert(has(r, "\"capabilities\":{\"tools\":{}}"));
    assert(has(r, "\"serverInfo\""));
    assert(has(r, "\"name\":\"axon-mcp\""));
    assert(has(r, "\"version\":"));
}

void test_initialize_version_mismatch_uses_ours()
{
    tests_run++;
    std::string r = rpc_handle_message(
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\","
        "\"params\":{\"protocolVersion\":\"1999-01-01\"}}");
    /* Client asked for a version we do not implement -> we answer with ours. */
    assert(has(r, "\"protocolVersion\":\"2025-06-18\""));
}

void test_ping()
{
    tests_run++;
    std::string r = rpc_handle_message("{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"ping\"}");
    assert(r == "{\"jsonrpc\":\"2.0\",\"id\":2,\"result\":{}}");
}

void test_tools_list_empty()
{
    tests_run++;
    std::string r = rpc_handle_message("{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/list\"}");
    assert(has(r, "\"tools\":[]"));
    assert(has(r, "\"id\":3"));
}

void test_notification_no_reply()
{
    tests_run++;
    std::string r = rpc_handle_message(
        "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}");
    assert(r.empty());   /* notifications get no response */
}

void test_notification_unknown_method_no_reply()
{
    tests_run++;
    /* No "id" -> notification, even for an unknown method: still no reply. */
    std::string r = rpc_handle_message("{\"jsonrpc\":\"2.0\",\"method\":\"whatever\"}");
    assert(r.empty());
}

void test_unknown_method_error()
{
    tests_run++;
    std::string r = rpc_handle_message("{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"bogus\"}");
    assert(has(r, "\"error\""));
    assert(has(r, "\"code\":-32601"));
    assert(has(r, "\"id\":4"));
}

void test_malformed_json_parse_error()
{
    tests_run++;
    std::string r = rpc_handle_message("{not valid json");
    assert(has(r, "\"code\":-32700"));
    assert(has(r, "\"id\":null"));   /* id unknown for unparseable input */
}

void test_oversized_input_rejected()
{
    tests_run++;
    std::string big(RPC_MAX_LINE_BYTES + 1, 'a');
    std::string r = rpc_handle_message(big);
    assert(has(r, "\"code\":-32700"));
}

void test_string_id_preserved()
{
    tests_run++;
    std::string r = rpc_handle_message("{\"jsonrpc\":\"2.0\",\"id\":\"abc\",\"method\":\"ping\"}");
    assert(has(r, "\"id\":\"abc\""));
}

int main()
{
    printf("=== MCP RPC Tests ===\n");

    TEST(initialize_shape);
    TEST(initialize_version_mismatch_uses_ours);
    TEST(ping);
    TEST(tools_list_empty);
    TEST(notification_no_reply);
    TEST(notification_unknown_method_no_reply);
    TEST(unknown_method_error);
    TEST(malformed_json_parse_error);
    TEST(oversized_input_rejected);
    TEST(string_id_preserved);

    printf("\n-----------------------\n");
    printf("%d Tests %d Failures 0 Ignored\n", tests_passed, tests_run - tests_passed);
    printf("OK\n");

    return 0;
}
