#include "error/error.h"

#include "memory/allocator.h"

GenLimits gen_default_limits(void)
{
    GenLimits limits;

    limits.max_source_size = GEN_DEFAULT_MAX_SOURCE_SIZE;
    limits.max_nesting_depth = GEN_DEFAULT_MAX_NESTING_DEPTH;
    limits.max_string_length = GEN_DEFAULT_MAX_STRING_LENGTH;
    limits.max_object_properties = GEN_DEFAULT_MAX_OBJECT_PROPERTIES;
    limits.max_list_length = GEN_DEFAULT_MAX_LIST_LENGTH;
    return limits;
}

void gen_error_reset(GenError *error)
{
    if (error == NULL) {
        return;
    }
    gen_free(error->message);
    error->message = NULL;
    gen_free(error->path);
    error->path = NULL;
    error->code = GEN_OK;
    error->line = 0;
    error->column = 0;
    error->offset = 0;
}

static char *gen_vprintf_alloc(const char *fmt, va_list args)
{
    va_list copy;
    int n;
    char *buf;

    va_copy(copy, args);
    n = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (n < 0) {
        return gen_strdup("error");
    }
    buf = (char *)gen_malloc((size_t)n + 1u);
    if (buf == NULL) {
        return NULL;
    }
    (void)vsnprintf(buf, (size_t)n + 1u, fmt, args);
    return buf;
}

void gen_context_set_errorv(
    GenContext *ctx,
    GenResult code,
    size_t line,
    size_t column,
    size_t offset,
    const char *fmt,
    va_list args
)
{
    char *msg;
    GenError extra;

    if (ctx == NULL) {
        return;
    }
    memset(&extra, 0, sizeof(extra));
    extra.code = code;
    extra.line = line;
    extra.column = column;
    extra.offset = offset;
    extra.path = ctx->source_path != NULL ? gen_strdup(ctx->source_path) : NULL;
    if (fmt == NULL) {
        extra.message = gen_strdup("error");
    } else {
        msg = gen_vprintf_alloc(fmt, args);
        extra.message = msg != NULL ? msg : gen_strdup("out of memory");
    }
    if (ctx->error_count + 1u > ctx->error_capacity) {
        size_t cap = ctx->error_capacity == 0 ? 4u : ctx->error_capacity * 2u;
        GenError *grown = (GenError *)gen_realloc(ctx->errors, cap * sizeof(GenError));
        if (grown == NULL) {
            if (ctx->last_error.code == GEN_OK) {
                ctx->last_error = extra;
            } else {
                gen_free(extra.message);
                gen_free(extra.path);
            }
            return;
        }
        ctx->errors = grown;
        ctx->error_capacity = cap;
    }
    ctx->errors[ctx->error_count++] = extra;
    if (ctx->last_error.code == GEN_OK) {
        ctx->last_error.code = extra.code;
        ctx->last_error.line = extra.line;
        ctx->last_error.column = extra.column;
        ctx->last_error.offset = extra.offset;
        ctx->last_error.message = gen_strdup(extra.message != NULL ? extra.message : "");
        ctx->last_error.path = extra.path != NULL ? gen_strdup(extra.path) : NULL;
    }
}

void gen_context_set_error(
    GenContext *ctx,
    GenResult code,
    size_t line,
    size_t column,
    size_t offset,
    const char *fmt,
    ...
)
{
    va_list args;

    va_start(args, fmt);
    gen_context_set_errorv(ctx, code, line, column, offset, fmt, args);
    va_end(args);
}

GenContext *gen_context_create(void)
{
    GenContext *ctx = (GenContext *)gen_calloc(1, sizeof(GenContext));
    if (ctx == NULL) {
        return NULL;
    }
    ctx->limits = gen_default_limits();
    ctx->last_error.code = GEN_OK;
    return ctx;
}

void gen_context_clear_error(GenContext *ctx)
{
    size_t i;
    if (ctx == NULL) {
        return;
    }
    gen_error_reset(&ctx->last_error);
    for (i = 0; i < ctx->error_count; i++) {
        gen_free(ctx->errors[i].message);
        gen_free(ctx->errors[i].path);
    }
    gen_free(ctx->errors);
    ctx->errors = NULL;
    ctx->error_count = 0;
    ctx->error_capacity = 0;
}

void gen_context_free(GenContext *ctx)
{
    if (ctx == NULL) {
        return;
    }
    gen_context_clear_error(ctx);
    gen_free(ctx);
}

const GenError *gen_context_last_error(const GenContext *ctx)
{
    if (ctx == NULL) {
        return NULL;
    }
    return &ctx->last_error;
}

size_t gen_context_error_count(const GenContext *ctx)
{
    if (ctx == NULL) {
        return 0;
    }
    return ctx->error_count;
}

const GenError *gen_context_error(const GenContext *ctx, size_t index)
{
    if (ctx == NULL || index >= ctx->error_count) {
        return NULL;
    }
    return &ctx->errors[index];
}

GenLimits gen_context_limits(const GenContext *ctx)
{
    if (ctx == NULL) {
        return gen_default_limits();
    }
    return ctx->limits;
}

GenResult gen_context_set_limits(GenContext *ctx, const GenLimits *limits)
{
    if (ctx == NULL || limits == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    if (limits->max_source_size == 0 ||
        limits->max_nesting_depth == 0 ||
        limits->max_string_length == 0 ||
        limits->max_object_properties == 0 ||
        limits->max_list_length == 0) {
        gen_context_set_error(
            ctx,
            GEN_ERR_INVALID_ARGUMENT,
            0,
            0,
            0,
            "resource limits must be greater than zero"
        );
        return GEN_ERR_INVALID_ARGUMENT;
    }
    ctx->limits = *limits;
    return GEN_OK;
}

GenResult gen_error_code(const GenError *error)
{
    if (error == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    return error->code;
}

const char *gen_error_message(const GenError *error)
{
    if (error == NULL || error->message == NULL) {
        return "";
    }
    return error->message;
}

size_t gen_error_line(const GenError *error)
{
    return error != NULL ? error->line : 0;
}

size_t gen_error_column(const GenError *error)
{
    return error != NULL ? error->column : 0;
}

size_t gen_error_offset(const GenError *error)
{
    return error != NULL ? error->offset : 0;
}

const char *gen_error_path(const GenError *error)
{
    if (error == NULL || error->path == NULL || error->path[0] == '\0') {
        return NULL;
    }
    return error->path;
}

const char *gen_version(void)
{
    return GENLANG_VERSION_STRING;
}

int gen_version_major(void)
{
    return GENLANG_VERSION_MAJOR;
}

int gen_version_minor(void)
{
    return GENLANG_VERSION_MINOR;
}

int gen_version_patch(void)
{
    return GENLANG_VERSION_PATCH;
}

void gen_string_free(char *string)
{
    gen_free(string);
}
