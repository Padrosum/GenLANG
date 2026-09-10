#ifndef GENLANG_INTERNAL_COMMON_H
#define GENLANG_INTERNAL_COMMON_H

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "genlang.h"

#define GEN_DEFAULT_MAX_SOURCE_SIZE ((size_t)16u * 1024u * 1024u)
#define GEN_DEFAULT_MAX_NESTING_DEPTH ((size_t)256u)
#define GEN_DEFAULT_MAX_STRING_LENGTH ((size_t)1024u * 1024u)
#define GEN_DEFAULT_MAX_OBJECT_PROPERTIES ((size_t)65536u)
#define GEN_DEFAULT_MAX_LIST_LENGTH ((size_t)65536u)

#define GEN_UNUSED(x) ((void)(x))

static inline size_t gen_align8(size_t n)
{
    return (n + (size_t)7u) & ~(size_t)7u;
}

#endif /* GENLANG_INTERNAL_COMMON_H */
