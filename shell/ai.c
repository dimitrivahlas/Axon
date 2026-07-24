#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "ai.h"

#define AI_TIMEOUT_SECS 30
#define RESPONSE_BUF_MAX 65536
#define PAYLOAD_MAX 131072

typedef struct {
    char data[RESPONSE_BUF_MAX];
    size_t len;
} response_buf_t;

static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    response_buf_t *buf = (response_buf_t *)userdata;
    size_t bytes = size * nmemb;

    if (buf->len + bytes >= RESPONSE_BUF_MAX - 1)
        bytes = RESPONSE_BUF_MAX - 1 - buf->len;

    memcpy(buf->data + buf->len, ptr, bytes);
    buf->len += bytes;
    buf->data[buf->len] = '\0';

    return size * nmemb;
}

static size_t json_escape(char *out, size_t max, const char *src)
{
    size_t pos = 0;
    for (const char *p = src; *p != '\0' && pos < max - 2; p++) {
        switch (*p) {
        case '"':  out[pos++] = '\\'; out[pos++] = '"'; break;
        case '\\': out[pos++] = '\\'; out[pos++] = '\\'; break;
        case '\n': out[pos++] = '\\'; out[pos++] = 'n'; break;
        case '\r': out[pos++] = '\\'; out[pos++] = 'r'; break;
        case '\t': out[pos++] = '\\'; out[pos++] = 't'; break;
        default:   out[pos++] = *p; break;
        }
    }
    out[pos] = '\0';
    return pos;
}

static const char *extract_text(const char *json)
{
    /* Find "text":" in the response and extract the value */
    const char *key = "\"text\":\"";
    const char *start = strstr(json, key);
    if (start == NULL)
        return NULL;

    start += strlen(key);
    return start;
}

/* Decode the escaped JSON string value at `start` (up to its closing quote)
 * into out, unescaping \n \" \\ \t. out is always NUL-terminated. */
static void decode_extracted_text(const char *start, char *out, size_t out_max)
{
    size_t pos = 0;
    const char *p = start;
    while (*p != '\0' && pos < out_max - 1) {
        if (*p == '"' && (p == start || *(p - 1) != '\\'))
            break;
        if (*p == '\\' && *(p + 1) == 'n') {
            out[pos++] = '\n';
            p += 2;
        } else if (*p == '\\' && *(p + 1) == '"') {
            out[pos++] = '"';
            p += 2;
        } else if (*p == '\\' && *(p + 1) == '\\') {
            out[pos++] = '\\';
            p += 2;
        } else if (*p == '\\' && *(p + 1) == 't') {
            out[pos++] = '\t';
            p += 2;
        } else {
            out[pos++] = *p;
            p++;
        }
    }
    out[pos] = '\0';
}

/* Perform the API call and decode Claude's reply text into `reply`.
 * Returns 0 on success, -1 on error. */
static int call_claude(const char *system_prompt, const char *user_msg,
                       char *reply, size_t reply_max)
{
    const char *api_key = getenv("ANTHROPIC_API_KEY");
    if (api_key == NULL || api_key[0] == '\0') {
        fprintf(stderr, "axon: ANTHROPIC_API_KEY not set\n");
        return -1;
    }

    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        fprintf(stderr, "axon: curl_easy_init failed\n");
        return -1;
    }

    char esc_system[4096], esc_user[32768];
    json_escape(esc_system, sizeof(esc_system), system_prompt);
    json_escape(esc_user, sizeof(esc_user), user_msg);

    char payload[PAYLOAD_MAX];
    snprintf(payload, sizeof(payload),
        "{"
        "\"model\":\"claude-sonnet-4-5-20250929\","
        "\"max_tokens\":1024,"
        "\"system\":\"%s\","
        "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]"
        "}",
        esc_system, esc_user);

    struct curl_slist *headers = NULL;
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "x-api-key: %s", api_key);
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "content-type: application/json");
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");

    response_buf_t response = { .data = "", .len = 0 };

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.anthropic.com/v1/messages");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)AI_TIMEOUT_SECS);

    fprintf(stderr, "\033[1;33mthinking...\033[0m\r");

    CURLcode res = curl_easy_perform(curl);

    /* Clear the "thinking..." line */
    fprintf(stderr, "              \r");

    if (res != CURLE_OK) {
        fprintf(stderr, "axon: API request failed: %s\n", curl_easy_strerror(res));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return -1;
    }

    long http_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (http_code != 200) {
        fprintf(stderr, "axon: API returned HTTP %ld\n", http_code);
        fprintf(stderr, "%s\n", response.data);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return -1;
    }

    const char *text = extract_text(response.data);
    if (text == NULL) {
        fprintf(stderr, "axon: could not parse API response\n");
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return -1;
    }

    decode_extracted_text(text, reply, reply_max);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return 0;
}

/* Print Claude's reply with the same decoration the shell has always used. */
static void print_reply(const char *reply)
{
    fprintf(stdout, "\n\033[1;33m--- axon ai ---\033[0m\n%s\n"
                    "\033[1;33m--- end ---\033[0m\n\n", reply);
}

int ai_ask(const char *question, const char *context_json, const char *cwd)
{
    char system_prompt[2048];
    snprintf(system_prompt, sizeof(system_prompt),
        "You are Axon, an AI assistant embedded in a Linux shell. "
        "The user is asking for help. You receive structured context including "
        "recent command history with exit codes and timing, error summaries, "
        "git state (branch, dirty files, recent commits), and session info. "
        "Be concise and direct. If a command failed, explain why and suggest a fix. "
        "Current working directory: %s",
        cwd);

    char user_msg[PAYLOAD_MAX];
    snprintf(user_msg, sizeof(user_msg),
        "Shell context: %s\n\nQuestion: %s",
        context_json, question);

    char reply[RESPONSE_BUF_MAX];
    if (call_claude(system_prompt, user_msg, reply, sizeof(reply)) != 0)
        return -1;

    print_reply(reply);
    return 0;
}

/* Trim leading/trailing whitespace (including newlines) in place. */
static void trim(char *s)
{
    char *start = s;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')
        start++;
    if (start != s)
        memmove(s, start, strlen(start) + 1);

    size_t len = strlen(s);
    while (len > 0) {
        char c = s[len - 1];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
            break;
        s[--len] = '\0';
    }
}

int ai_suggest(const char *intent, const char *context_json, const char *cwd,
               char *out_command, size_t out_max)
{
    if (out_command != NULL && out_max > 0)
        out_command[0] = '\0';

    char system_prompt[2048];
    snprintf(system_prompt, sizeof(system_prompt),
        "You are Axon, an AI assistant embedded in a Linux shell. "
        "The user describes what they want to do. You receive structured context "
        "including recent command history, error summaries, git state, and session info. "
        "Suggest a single shell command. "
        "Reply with ONLY the command, no explanation, no markdown, no backticks. "
        "Current working directory: %s",
        cwd);

    char user_msg[PAYLOAD_MAX];
    snprintf(user_msg, sizeof(user_msg),
        "Shell context: %s\n\nI want to: %s",
        context_json, intent);

    char reply[RESPONSE_BUF_MAX];
    if (call_claude(system_prompt, user_msg, reply, sizeof(reply)) != 0)
        return -1;

    trim(reply);
    print_reply(reply);

    if (out_command != NULL && out_max > 0) {
        strncpy(out_command, reply, out_max - 1);
        out_command[out_max - 1] = '\0';
    }
    return 0;
}
