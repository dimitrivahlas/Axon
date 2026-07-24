#ifndef AXON_MCP_RPC_H
#define AXON_MCP_RPC_H

#include <cstddef>
#include <string>

/* Maximum accepted message length. Longer lines are rejected with a parse
 * error rather than parsed, bounding memory and work per message. */
constexpr std::size_t RPC_MAX_LINE_BYTES = 1024 * 1024;  /* 1 MB */

/*
 * Handle a single JSON-RPC 2.0 message (one line of input).
 *
 * Returns the serialized JSON response to write back, or an empty string when
 * there is nothing to reply — i.e. for notifications (requests with no "id").
 *
 * This function does no I/O and keeps no state, so it can be unit-tested
 * directly. It rejects input longer than RPC_MAX_LINE_BYTES; the stdio loop in
 * main.cpp still reads defensively so it never buffers an unbounded line.
 */
std::string rpc_handle_message(const std::string &line);

#endif
