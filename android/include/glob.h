// Stub glob implementation for Android API < 28
// glob() is only available from API 28+. Pure Data uses it in x_file.c for
// file-pattern matching in patches, which is not needed for embedded audio.
// These stubs compile cleanly and always return "no matches".
#pragma once
#include <stddef.h>

typedef struct {
    size_t   gl_pathc;
    char   **gl_pathv;
    size_t   gl_offs;
} glob_t;

#define GLOB_NOSORT   0
#define GLOB_MARK     0
#define GLOB_APPEND   0
#define GLOB_NOCHECK  0
#define GLOB_NOESCAPE 0
#define GLOB_NOMATCH  1

static inline int glob(const char *pattern, int flags,
    int (*errfunc)(const char *epath, int eerrno), glob_t *pglob)
{
    (void)pattern; (void)flags; (void)errfunc;
    pglob->gl_pathc = 0;
    pglob->gl_pathv = 0;
    pglob->gl_offs  = 0;
    return GLOB_NOMATCH;
}

static inline void globfree(glob_t *pglob)
{
    (void)pglob;
}
