#include "git_context.h"

/* Stub — real git CLI integration comes in Step 4 */

GitContext git_gather_context(const std::string &, int)
{
    GitContext ctx;
    ctx.is_dirty = false;
    ctx.is_git_repo = false;
    return ctx;
}
