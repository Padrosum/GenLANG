#ifndef GENLANG_ERROR_H
#define GENLANG_ERROR_H

#include "internal/common.h"

struct GenError {
    GenResult code;
    char *message;
    char *path;
    size_t line;
    size_t column;
    size_t offset;
};

struct GenContext {
    struct GenError last_error;
    GenError *errors;
    size_t error_count;
    size_t error_capacity;
    GenLimits limits;
    const char *source_path;
};

void gen_error_reset(GenError *error);
void gen_context_set_error(
    GenContext *ctx,
    GenResult code,
    size_t line,
    size_t column,
    size_t offset,
    const char *fmt,
    ...
);
void gen_context_set_errorv(
    GenContext *ctx,
    GenResult code,
    size_t line,
    size_t column,
    size_t offset,
    const char *fmt,
    va_list args
);

GenLimits gen_default_limits(void);

#endif /* GENLANG_ERROR_H */
