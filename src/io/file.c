#include "io/file.h"

#include "error/error.h"
#include "memory/allocator.h"
#include "serializer/serializer.h"

GenResult gen_io_read_file(
    GenContext *ctx,
    const char *path,
    char **out_data,
    size_t *out_length
)
{
    FILE *fp;
    char *buf;
    size_t cap;
    size_t nread;
    size_t used = 0;
    long size;

    if (ctx == NULL || path == NULL || out_data == NULL || out_length == NULL) {
        if (ctx != NULL) {
            gen_context_set_error(ctx, GEN_ERR_INVALID_ARGUMENT, 0, 0, 0, "invalid argument");
        }
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_data = NULL;
    *out_length = 0;
    fp = fopen(path, "rb");
    if (fp == NULL) {
        gen_context_set_error(ctx, GEN_ERR_IO, 0, 0, 0, "cannot open file '%s'", path);
        return GEN_ERR_IO;
    }

    size = -1;
    if (fseek(fp, 0, SEEK_END) == 0) {
        size = ftell(fp);
        if (size >= 0 && fseek(fp, 0, SEEK_SET) != 0) {
            size = -1;
        }
    }
    if (size < 0) {
        cap = 4096;
        buf = (char *)gen_malloc(cap);
        if (buf == NULL) {
            fclose(fp);
            gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
            return GEN_ERR_OUT_OF_MEMORY;
        }
        for (;;) {
            if (used + 1u >= cap) {
                char *grown;
                size_t next = cap * 2u;
                if (next > ctx->limits.max_source_size + 1u) {
                    next = ctx->limits.max_source_size + 1u;
                }
                if (next <= cap) {
                    gen_free(buf);
                    fclose(fp);
                    gen_context_set_error(
                        ctx, GEN_ERR_IO, 0, 0, 0, "file '%s' exceeds maximum source size", path
                    );
                    return GEN_ERR_IO;
                }
                grown = (char *)gen_realloc(buf, next);
                if (grown == NULL) {
                    gen_free(buf);
                    fclose(fp);
                    gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
                    return GEN_ERR_OUT_OF_MEMORY;
                }
                buf = grown;
                cap = next;
            }
            nread = fread(buf + used, 1, cap - used - 1u, fp);
            used += nread;
            if (nread == 0) {
                break;
            }
            if (used > ctx->limits.max_source_size) {
                gen_free(buf);
                fclose(fp);
                gen_context_set_error(
                    ctx, GEN_ERR_IO, 0, 0, 0, "file '%s' exceeds maximum source size", path
                );
                return GEN_ERR_IO;
            }
        }
        fclose(fp);
        buf[used] = '\0';
        *out_data = buf;
        *out_length = used;
        return GEN_OK;
    }

    if ((size_t)size > ctx->limits.max_source_size) {
        fclose(fp);
        gen_context_set_error(
            ctx, GEN_ERR_IO, 0, 0, 0, "file '%s' exceeds maximum source size", path
        );
        return GEN_ERR_IO;
    }
    buf = (char *)gen_malloc((size_t)size + 1u);
    if (buf == NULL) {
        fclose(fp);
        gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
        return GEN_ERR_OUT_OF_MEMORY;
    }
    nread = fread(buf, 1, (size_t)size, fp);
    fclose(fp);
    if (nread != (size_t)size) {
        gen_free(buf);
        gen_context_set_error(ctx, GEN_ERR_IO, 0, 0, 0, "failed to read file '%s'", path);
        return GEN_ERR_IO;
    }
    buf[nread] = '\0';
    *out_data = buf;
    *out_length = nread;
    return GEN_OK;
}

GenResult gen_io_write_file(GenContext *ctx, const char *path, const char *data, size_t length)
{
    FILE *fp;
    size_t nwrite;

    if (ctx == NULL || path == NULL || data == NULL) {
        if (ctx != NULL) {
            gen_context_set_error(ctx, GEN_ERR_INVALID_ARGUMENT, 0, 0, 0, "invalid argument");
        }
        return GEN_ERR_INVALID_ARGUMENT;
    }
    fp = fopen(path, "wb");
    if (fp == NULL) {
        gen_context_set_error(ctx, GEN_ERR_IO, 0, 0, 0, "cannot write file '%s'", path);
        return GEN_ERR_IO;
    }
    nwrite = fwrite(data, 1, length, fp);
    if (nwrite != length || fclose(fp) != 0) {
        gen_context_set_error(ctx, GEN_ERR_IO, 0, 0, 0, "failed to write file '%s'", path);
        return GEN_ERR_IO;
    }
    return GEN_OK;
}

GenResult gen_document_load_file(
    GenContext *ctx,
    const char *path,
    GenDocument **out_document
)
{
    char *data = NULL;
    size_t length = 0;
    GenResult rc;

    rc = gen_io_read_file(ctx, path, &data, &length);
    if (rc != GEN_OK) {
        return rc;
    }
    GEN_UNUSED(length);
    rc = gen_document_parse_at(ctx, data, path, out_document);
    gen_free(data);
    return rc;
}

GenResult gen_document_save_file(
    GenContext *ctx,
    const char *path,
    const GenDocument *document
)
{
    char *text = NULL;
    size_t length = 0;
    GenResult rc;

    if (ctx == NULL || path == NULL || document == NULL) {
        if (ctx != NULL) {
            gen_context_set_error(ctx, GEN_ERR_INVALID_ARGUMENT, 0, 0, 0, "invalid argument");
        }
        return GEN_ERR_INVALID_ARGUMENT;
    }
    rc = gen_document_serialize(document, &text, &length);
    if (rc != GEN_OK) {
        gen_context_set_error(ctx, rc, 0, 0, 0, "serialization failed");
        return rc;
    }
    rc = gen_io_write_file(ctx, path, text, length);
    gen_string_free(text);
    return rc;
}
