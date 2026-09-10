#ifndef GENLANG_IO_H
#define GENLANG_IO_H

#include "internal/common.h"

GenResult gen_io_read_file(
    GenContext *ctx,
    const char *path,
    char **out_data,
    size_t *out_length
);
GenResult gen_io_write_file(GenContext *ctx, const char *path, const char *data, size_t length);

#endif /* GENLANG_IO_H */
