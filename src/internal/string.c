#include "internal/string.h"

#include "internal/common.h"
#include "memory/allocator.h"

bool gen_string_init_copy(GenString *s, const char *data, size_t length)
{
    if (s == NULL) {
        return false;
    }
    s->data = NULL;
    s->length = 0;
    if (data == NULL) {
        s->data = gen_strdup("");
        return s->data != NULL;
    }
    s->data = gen_strndup(data, length);
    if (s->data == NULL) {
        return false;
    }
    s->length = length;
    return true;
}

void gen_string_clear(GenString *s)
{
    if (s == NULL) {
        return;
    }
    gen_free(s->data);
    s->data = NULL;
    s->length = 0;
}

void gen_strbuf_init(GenStrBuf *buf)
{
    if (buf == NULL) {
        return;
    }
    buf->data = NULL;
    buf->length = 0;
    buf->capacity = 0;
}

void gen_strbuf_free(GenStrBuf *buf)
{
    if (buf == NULL) {
        return;
    }
    gen_free(buf->data);
    buf->data = NULL;
    buf->length = 0;
    buf->capacity = 0;
}

bool gen_strbuf_reserve(GenStrBuf *buf, size_t needed)
{
    char *grown;
    size_t cap;

    if (buf == NULL) {
        return false;
    }
    if (needed <= buf->capacity) {
        return true;
    }
    cap = buf->capacity == 0 ? 64u : buf->capacity;
    while (cap < needed) {
        if (cap > SIZE_MAX / 2u) {
            cap = needed;
            break;
        }
        cap *= 2u;
    }
    grown = (char *)gen_realloc(buf->data, cap);
    if (grown == NULL) {
        return false;
    }
    buf->data = grown;
    buf->capacity = cap;
    return true;
}

bool gen_strbuf_append(GenStrBuf *buf, const char *data, size_t length)
{
    if (buf == NULL) {
        return false;
    }
    if (length == 0) {
        return true;
    }
    if (data == NULL) {
        return false;
    }
    if (!gen_strbuf_reserve(buf, buf->length + length + 1u)) {
        return false;
    }
    memcpy(buf->data + buf->length, data, length);
    buf->length += length;
    buf->data[buf->length] = '\0';
    return true;
}

bool gen_strbuf_append_cstr(GenStrBuf *buf, const char *data)
{
    if (data == NULL) {
        return true;
    }
    return gen_strbuf_append(buf, data, strlen(data));
}

bool gen_strbuf_append_char(GenStrBuf *buf, char c)
{
    return gen_strbuf_append(buf, &c, 1u);
}

bool gen_strbuf_appendfv(GenStrBuf *buf, const char *fmt, va_list args)
{
    va_list copy;
    int n;
    size_t need;

    if (buf == NULL || fmt == NULL) {
        return false;
    }
    va_copy(copy, args);
    n = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (n < 0) {
        return false;
    }
    need = buf->length + (size_t)n + 1u;
    if (!gen_strbuf_reserve(buf, need)) {
        return false;
    }
    n = vsnprintf(buf->data + buf->length, (size_t)n + 1u, fmt, args);
    if (n < 0) {
        return false;
    }
    buf->length += (size_t)n;
    return true;
}

bool gen_strbuf_appendf(GenStrBuf *buf, const char *fmt, ...)
{
    va_list args;
    bool ok;

    va_start(args, fmt);
    ok = gen_strbuf_appendfv(buf, fmt, args);
    va_end(args);
    return ok;
}

char *gen_strbuf_steal(GenStrBuf *buf, size_t *out_length)
{
    char *data;

    if (buf == NULL) {
        return NULL;
    }
    if (buf->data == NULL) {
        data = gen_strdup("");
        if (out_length != NULL) {
            *out_length = 0;
        }
        return data;
    }
    if (out_length != NULL) {
        *out_length = buf->length;
    }
    data = buf->data;
    buf->data = NULL;
    buf->length = 0;
    buf->capacity = 0;
    return data;
}

bool gen_utf8_validate(const char *data, size_t length)
{
    size_t i = 0;

    if (data == NULL) {
        return length == 0;
    }
    while (i < length) {
        unsigned char c = (unsigned char)data[i];
        size_t remain;
        unsigned int cp;

        if (c <= 0x7Fu) {
            i++;
            continue;
        }
        if ((c & 0xE0u) == 0xC0u) {
            remain = 1;
            cp = (unsigned int)(c & 0x1Fu);
            if (c < 0xC2u) {
                return false;
            }
        } else if ((c & 0xF0u) == 0xE0u) {
            remain = 2;
            cp = (unsigned int)(c & 0x0Fu);
        } else if ((c & 0xF8u) == 0xF0u) {
            remain = 3;
            cp = (unsigned int)(c & 0x07u);
            if (c > 0xF4u) {
                return false;
            }
        } else {
            return false;
        }
        if (i + remain >= length) {
            return false;
        }
        for (size_t j = 1; j <= remain; j++) {
            unsigned char cc = (unsigned char)data[i + j];
            if ((cc & 0xC0u) != 0x80u) {
                return false;
            }
            cp = (cp << 6) | (unsigned int)(cc & 0x3Fu);
        }
        if (cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
            return false;
        }
        if (remain == 2 && cp < 0x800u) {
            return false;
        }
        if (remain == 3 && cp < 0x10000u) {
            return false;
        }
        i += remain + 1u;
    }
    return true;
}

bool gen_utf8_next(
    const char *data,
    size_t length,
    size_t offset,
    uint32_t *out_cp,
    size_t *out_size
)
{
    unsigned char c;
    size_t remain;
    unsigned int cp;
    size_t j;

    if (data == NULL || offset >= length) {
        return false;
    }
    c = (unsigned char)data[offset];
    if (c <= 0x7Fu) {
        if (out_cp != NULL) {
            *out_cp = c;
        }
        if (out_size != NULL) {
            *out_size = 1;
        }
        return true;
    }
    if ((c & 0xE0u) == 0xC0u) {
        remain = 1;
        cp = (unsigned int)(c & 0x1Fu);
        if (c < 0xC2u) {
            return false;
        }
    } else if ((c & 0xF0u) == 0xE0u) {
        remain = 2;
        cp = (unsigned int)(c & 0x0Fu);
    } else if ((c & 0xF8u) == 0xF0u) {
        remain = 3;
        cp = (unsigned int)(c & 0x07u);
        if (c > 0xF4u) {
            return false;
        }
    } else {
        return false;
    }
    if (offset + remain >= length) {
        return false;
    }
    for (j = 1; j <= remain; j++) {
        unsigned char cc = (unsigned char)data[offset + j];
        if ((cc & 0xC0u) != 0x80u) {
            return false;
        }
        cp = (cp << 6) | (unsigned int)(cc & 0x3Fu);
    }
    if (cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
        return false;
    }
    if (remain == 2 && cp < 0x800u) {
        return false;
    }
    if (remain == 3 && cp < 0x10000u) {
        return false;
    }
    if (out_cp != NULL) {
        *out_cp = (uint32_t)cp;
    }
    if (out_size != NULL) {
        *out_size = remain + 1u;
    }
    return true;
}

static bool gen_utf8_is_space_cp(uint32_t cp)
{
    return cp == 0x00A0u || cp == 0x1680u || (cp >= 0x2000u && cp <= 0x200Au) ||
           cp == 0x2028u || cp == 0x2029u || cp == 0x202Fu || cp == 0x205Fu ||
           cp == 0x3000u || cp == 0xFEFFu;
}

bool gen_utf8_is_ident_start(uint32_t cp)
{
    if (cp < 0x80u) {
        return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') || cp == '_';
    }
    if (cp < 0xA0u || gen_utf8_is_space_cp(cp)) {
        return false;
    }
    return true;
}

bool gen_utf8_is_ident_continue(uint32_t cp)
{
    if (cp < 0x80u) {
        return gen_utf8_is_ident_start(cp) || (cp >= '0' && cp <= '9');
    }
    return gen_utf8_is_ident_start(cp);
}
