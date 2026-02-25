#include "context.h"
#include "storage.h"
#include "git_context.h"

#include <cstdlib>
#include <cstring>

/* Stubs — real implementations come in Steps 2-5 */

extern "C" {

int context_init(void)
{
    return 0;
}

int context_add(const char *, int, double, const char *, const char *)
{
    return 0;
}

char *context_build_ai_json(const char *, int)
{
    /* Return minimal valid JSON so callers don't crash */
    const char *empty = "{}";
    char *result = static_cast<char *>(malloc(strlen(empty) + 1));
    if (result)
        strcpy(result, empty);
    return result;
}

void context_shutdown(void)
{
}

const char *context_session_id(void)
{
    return NULL;
}

} /* extern "C" */
