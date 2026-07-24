#include "rpc.h"
#include "json_value.h"

/* Protocol version we implement. On initialize we echo the client's requested
 * version if it matches, otherwise we answer with ours. */
static const char *MCP_PROTOCOL_VERSION = "2025-06-18";
static const char *AXON_MCP_NAME = "axon-mcp";
static const char *AXON_MCP_VERSION = "0.1.0";

/* JSON-RPC 2.0 error codes used by the scaffold. */
enum {
    RPC_PARSE_ERROR      = -32700,
    RPC_METHOD_NOT_FOUND = -32601,
    RPC_INVALID_PARAMS   = -32602,
};

/* Build {"jsonrpc":"2.0","id":<id>,"result":<result>}. */
static std::string make_success(const JsonValue &id, JsonValue result)
{
    JsonValue resp = JsonValue::object();
    resp.set("jsonrpc", JsonValue::string("2.0"));
    resp.set("id", id);
    resp.set("result", std::move(result));
    return resp.dump();
}

/* Build {"jsonrpc":"2.0","id":<id>,"error":{"code":..,"message":..}}. */
static std::string make_error(const JsonValue &id, int code, const std::string &message)
{
    JsonValue err = JsonValue::object();
    err.set("code", JsonValue::integer(code));
    err.set("message", JsonValue::string(message));

    JsonValue resp = JsonValue::object();
    resp.set("jsonrpc", JsonValue::string("2.0"));
    resp.set("id", id);
    resp.set("error", std::move(err));
    return resp.dump();
}

std::string rpc_handle_message(const std::string &line)
{
    /* 0. Reject over-long input before doing any work. */
    if (line.size() > RPC_MAX_LINE_BYTES)
        return make_error(JsonValue::null(), RPC_PARSE_ERROR, "Line too long");

    /* 1. Parse. Malformed JSON -> parse error, id unknown (null). */
    std::string perr;
    JsonValue msg = JsonValue::parse(line, perr);
    if (!perr.empty())
        return make_error(JsonValue::null(), RPC_PARSE_ERROR, "Parse error");

    if (!msg.is_object())
        return make_error(JsonValue::null(), RPC_METHOD_NOT_FOUND, "Method not found");

    /* A message with no "id" is a notification and never gets a reply. */
    const JsonValue *id_v = msg.get_object_member("id");
    bool is_notification = (id_v == nullptr);
    JsonValue id = id_v ? *id_v : JsonValue::null();

    /* 2. Require a string "method". */
    const JsonValue *method_v = msg.get_object_member("method");
    if (method_v == nullptr || !method_v->is_string()) {
        if (is_notification)
            return "";   /* malformed notification: ignore silently */
        return make_error(id, RPC_METHOD_NOT_FOUND, "Method not found");
    }
    std::string method = method_v->get_string();

    const JsonValue *params_v = msg.get_object_member("params");
    JsonValue params = params_v ? *params_v : JsonValue::object();

    /* 3. Notifications: act (if known) but never reply. */
    if (is_notification) {
        /* notifications/initialized (and any other) -> no output. */
        return "";
    }

    /* 4. Requests. */
    if (method == "initialize") {
        std::string ver = MCP_PROTOCOL_VERSION;
        const JsonValue *pv = params.get_object_member("protocolVersion");
        if (pv && pv->is_string() && pv->get_string() == MCP_PROTOCOL_VERSION)
            ver = pv->get_string();   /* echo the client's version when it matches */

        JsonValue caps = JsonValue::object();
        caps.set("tools", JsonValue::object());

        JsonValue info = JsonValue::object();
        info.set("name", JsonValue::string(AXON_MCP_NAME));
        info.set("version", JsonValue::string(AXON_MCP_VERSION));

        JsonValue result = JsonValue::object();
        result.set("protocolVersion", JsonValue::string(ver));
        result.set("capabilities", std::move(caps));
        result.set("serverInfo", std::move(info));
        return make_success(id, std::move(result));
    }

    if (method == "ping")
        return make_success(id, JsonValue::object());

    if (method == "tools/list") {
        JsonValue result = JsonValue::object();
        result.set("tools", JsonValue::array());   /* real tools arrive in Step 16 */
        return make_success(id, std::move(result));
    }

    return make_error(id, RPC_METHOD_NOT_FOUND, "Method not found");
}
