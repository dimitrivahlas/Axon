#include <cstdio>
#include <string>
#include "rpc.h"

/*
 * axon-mcp — MCP server entry point.
 *
 * Speaks JSON-RPC 2.0 over stdio: reads newline-delimited messages from stdin,
 * hands each to rpc_handle_message, and writes any reply to stdout followed by
 * a newline, flushing after each one. stdout carries protocol messages ONLY;
 * anything else (logging, errors) must go to stderr.
 */

static void write_response(const std::string &resp)
{
    if (resp.empty())
        return;   /* notifications produce no reply */
    std::fputs(resp.c_str(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);   /* don't let a reply sit in the buffer */
}

static void process_line(const std::string &line)
{
    if (line.empty())
        return;   /* skip blank lines between messages */
    write_response(rpc_handle_message(line));
}

int main()
{
    std::string line;
    int c;

    /*
     * Read one character at a time and assemble lines. Accumulation is bounded
     * at RPC_MAX_LINE_BYTES + 1 so a line with no newline (accidental or
     * hostile) can never balloon memory: past the cap we stop storing and drain
     * to the next newline, and rpc_handle_message reports the over-long line as
     * a parse error.
     */
    while ((c = std::getchar()) != EOF) {
        if (c == '\n') {
            process_line(line);
            line.clear();
        } else if (line.size() <= RPC_MAX_LINE_BYTES) {
            line.push_back(static_cast<char>(c));
        }
        /* else: over the cap — drop this byte until the line ends */
    }

    /* A final message with no trailing newline before EOF. */
    process_line(line);

    return 0;
}
