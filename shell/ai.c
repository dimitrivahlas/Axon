#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "ai.h"

#define AI_TIMEOUT_SECS 30
#define RESPONSE_BUF_MAX 65536
#define PAYLOAD_MAX 32768

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

static void build_history_json(char *out, size_t max, history_t *hist)
{
    size_t pos = 0;
    pos += (size_t)snprintf(out + pos, max - pos, "[");

    for (int i = 0; i < hist->count; i++) {
        history_entry_t *e = &hist->entries[i];
        if (i > 0)
            pos += (size_t)snprintf(out + pos, max - pos, ",");
        pos += (size_t)snprintf(out + pos, max - pos,
            "{\"command\":\"%s\",\"exit_code\":%d,\"elapsed_ms\":%.1f,\"cwd\":\"%s\"}",
            e->raw_line, e->exit_code, e->elapsed_ms, e->cwd);
    }

    snprintf(out + pos, max - pos, "]");
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

static void print_extracted_text(const char *start)
{
    fprintf(stdout, "\n\033[1;33m--- axon ai ---\033[0m\n");

    const char *p = start;
    while (*p != '\0') {
        if (*p == '"' && (p == start || *(p - 1) != '\\'))
            break;
        if (*p == '\\' && *(p + 1) == 'n') {
            fputc('\n', stdout);
            p += 2;
        } else if (*p == '\\' && *(p + 1) == '"') {
            fputc('"', stdout);
            p += 2;
        } else if (*p == '\\' && *(p + 1) == '\\') {
            fputc('\\', stdout);
            p += 2;
        } else if (*p == '\\' && *(p + 1) == 't') {
            fputc('\t', stdout);
            p += 2;
        } else {
            fputc(*p, stdout);
            p++;
        }
    }

    fprintf(stdout, "\n\033[1;33m--- end ---\033[0m\n\n");
}

static int call_claude(const char *system_prompt, const char *user_msg)
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

    /* Build JSON payload — escape is minimal since we control the inputs */
    char payload[PAYLOAD_MAX];
    snprintf(payload, sizeof(payload),
        "{"
        "\"model\":\"claude-sonnet-4-5-20250929\","
        "\"max_tokens\":1024,"
        "\"system\":\"%s\","
        "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]"
        "}",
        system_prompt, user_msg);

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

    print_extracted_text(text);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return 0;
}

int ai_ask(const char *question, history_t *hist, const char *cwd)
{
    char history_json[16384];
    build_history_json(history_json, sizeof(history_json), hist);

    char system_prompt[2048];
    snprintf(system_prompt, sizeof(system_prompt),
        "You are Axon, an AI assistant embedded in a Linux shell. "
        "The user is asking for help. You have access to their recent "
        "command history with exit codes and timing. Be concise and direct. "
        "If a command failed, explain why and suggest a fix. "
        "Current working directory: %s",
        cwd);

    char user_msg[PAYLOAD_MAX];
    snprintf(user_msg, sizeof(user_msg),
        "Recent command history: %s\\n\\nQuestion: %s",
        history_json, question);

    return call_claude(system_prompt, user_msg);
}

int ai_suggest(const char *intent, history_t *hist, const char *cwd)
{
    char history_json[16384];
    build_history_json(history_json, sizeof(history_json), hist);

    char system_prompt[2048];
    snprintf(system_prompt, sizeof(system_prompt),
        "You are Axon, an AI assistant embedded in a Linux shell. "
        "The user describes what they want to do. Suggest a single shell command. "
        "Reply with ONLY the command, no explanation, no markdown, no backticks. "
        "Current working directory: %s",
        cwd);

    char user_msg[PAYLOAD_MAX];
    snprintf(user_msg, sizeof(user_msg),
        "Recent command history: %s\\n\\nI want to: %s",
        history_json, intent);

    return call_claude(system_prompt, user_msg);
}
