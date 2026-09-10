#ifndef GENLANG_INTERNAL_STRING_H
#define GENLANG_INTERNAL_STRING_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char *data;
    size_t length;
} GenString;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} GenStrBuf;

bool gen_string_init_copy(GenString *s, const char *data, size_t length);
void gen_string_clear(GenString *s);

void gen_strbuf_init(GenStrBuf *buf);
void gen_strbuf_free(GenStrBuf *buf);
bool gen_strbuf_reserve(GenStrBuf *buf, size_t needed);
bool gen_strbuf_append(GenStrBuf *buf, const char *data, size_t length);
bool gen_strbuf_append_cstr(GenStrBuf *buf, const char *data);
bool gen_strbuf_append_char(GenStrBuf *buf, char c);
bool gen_strbuf_appendf(GenStrBuf *buf, const char *fmt, ...);
bool gen_strbuf_appendfv(GenStrBuf *buf, const char *fmt, va_list args);
char *gen_strbuf_steal(GenStrBuf *buf, size_t *out_length);

bool gen_utf8_validate(const char *data, size_t length);
bool gen_utf8_next(
    const char *data,
    size_t length,
    size_t offset,
    uint32_t *out_cp,
    size_t *out_size
);
bool gen_utf8_is_ident_start(uint32_t cp);
bool gen_utf8_is_ident_continue(uint32_t cp);

#endif /* GENLANG_INTERNAL_STRING_H */
