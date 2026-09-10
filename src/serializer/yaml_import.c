#include "serializer/yaml.h"

#include "error/error.h"
#include "memory/allocator.h"

typedef struct {
    GenContext *ctx;
    const char *src;
    size_t length;
    size_t pos;
    size_t depth;
    int failed;
} YamlParser;

static int yaml_failed(const YamlParser *p)
{
    return p->failed != 0;
}

static void yaml_fail(YamlParser *p, const char *fmt, ...)
{
    va_list args;
    size_t line = 1;
    size_t col = 1;
    size_t i;

    for (i = 0; i < p->pos && i < p->length; i++) {
        if (p->src[i] == '\n') {
            line++;
            col = 1;
        } else {
            col++;
        }
    }
    p->failed = 1;
    va_start(args, fmt);
    gen_context_set_errorv(p->ctx, GEN_ERR_SERIALIZATION, line, col, p->pos, fmt, args);
    va_end(args);
}

static int yaml_peek(const YamlParser *p)
{
    if (p->pos >= p->length) {
        return -1;
    }
    return (unsigned char)p->src[p->pos];
}

static void yaml_skip_spaces(YamlParser *p)
{
    while (p->pos < p->length) {
        char c = p->src[p->pos];
        if (c != ' ' && c != '\t') {
            break;
        }
        if (c == '\t') {
            yaml_fail(p, "tabs are not allowed in YAML indentation");
            return;
        }
        p->pos++;
    }
}

static void yaml_skip_to_eol(YamlParser *p)
{
    yaml_skip_spaces(p);
    if (yaml_peek(p) == '#') {
        while (p->pos < p->length && p->src[p->pos] != '\n' && p->src[p->pos] != '\r') {
            p->pos++;
        }
    }
    if (p->pos < p->length && p->src[p->pos] == '\r') {
        p->pos++;
    }
    if (p->pos < p->length && p->src[p->pos] == '\n') {
        p->pos++;
    }
}

static void yaml_skip_blanks(YamlParser *p)
{
    for (;;) {
        size_t saved;
        if (yaml_failed(p)) {
            return;
        }
        saved = p->pos;
        yaml_skip_spaces(p);
        if (yaml_failed(p)) {
            return;
        }
        if (yaml_peek(p) == '#') {
            yaml_skip_to_eol(p);
            continue;
        }
        if (yaml_peek(p) == '\r' || yaml_peek(p) == '\n') {
            yaml_skip_to_eol(p);
            continue;
        }
        p->pos = saved;
        yaml_skip_spaces(p);
        return;
    }
}

static size_t yaml_line_indent(const YamlParser *p)
{
    size_t i = p->pos;
    size_t indent = 0;

    while (i > 0 && p->src[i - 1u] != '\n') {
        i--;
    }
    while (i < p->length && p->src[i] == ' ') {
        indent++;
        i++;
    }
    return indent;
}

static size_t yaml_column(const YamlParser *p)
{
    size_t start = p->pos;
    while (start > 0 && p->src[start - 1u] != '\n') {
        start--;
    }
    return p->pos - start;
}

static int yaml_starts_with(const YamlParser *p, const char *word)
{
    size_t n = strlen(word);
    if (p->pos + n > p->length) {
        return 0;
    }
    return memcmp(p->src + p->pos, word, n) == 0;
}

static bool yaml_json_quote_append(GenStrBuf *buf, const char *data, size_t length)
{
    size_t i;
    if (!gen_strbuf_append_char(buf, '"')) {
        return false;
    }
    for (i = 0; i < length; i++) {
        unsigned char c = (unsigned char)data[i];
        if (c == '"' || c == '\\') {
            if (!gen_strbuf_append_char(buf, '\\') || !gen_strbuf_append_char(buf, (char)c)) {
                return false;
            }
        } else if (c == '\n') {
            if (!gen_strbuf_append_cstr(buf, "\\n")) {
                return false;
            }
        } else if (c == '\t') {
            if (!gen_strbuf_append_cstr(buf, "\\t")) {
                return false;
            }
        } else if (c == '\r') {
            if (!gen_strbuf_append_cstr(buf, "\\r")) {
                return false;
            }
        } else if (c < 0x20u) {
            if (!gen_strbuf_appendf(buf, "\\u%04x", (unsigned int)c)) {
                return false;
            }
        } else if (!gen_strbuf_append_char(buf, (char)c)) {
            return false;
        }
    }
    return gen_strbuf_append_char(buf, '"');
}

static GenResult yaml_parse_node(YamlParser *p, GenStrBuf *out, size_t min_indent);

static GenResult yaml_copy_flow(YamlParser *p, GenStrBuf *out)
{
    int curly = 0;
    int square = 0;
    int in_string = 0;
    int escape = 0;

    if (yaml_peek(p) == '{') {
        curly = 1;
    } else if (yaml_peek(p) == '[') {
        square = 1;
    } else {
        yaml_fail(p, "expected flow collection");
        return GEN_ERR_SERIALIZATION;
    }
    if (!gen_strbuf_append_char(out, p->src[p->pos])) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    p->pos++;
    while (p->pos < p->length) {
        unsigned char c = (unsigned char)p->src[p->pos++];
        if (!gen_strbuf_append_char(out, (char)c)) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (in_string) {
            if (escape) {
                escape = 0;
            } else if (c == '\\') {
                escape = 1;
            } else if (c == '"') {
                in_string = 0;
            }
            continue;
        }
        if (c == '"') {
            in_string = 1;
        } else if (c == '{') {
            curly++;
        } else if (c == '}') {
            curly--;
        } else if (c == '[') {
            square++;
        } else if (c == ']') {
            square--;
        }
        if (curly < 0 || square < 0) {
            yaml_fail(p, "unbalanced flow collection");
            return GEN_ERR_SERIALIZATION;
        }
        if (curly == 0 && square == 0) {
            return GEN_OK;
        }
    }
    yaml_fail(p, "unterminated flow collection");
    return GEN_ERR_SERIALIZATION;
}

static GenResult yaml_parse_double_quoted(YamlParser *p, GenStrBuf *out)
{
    GenStrBuf raw;
    char *stolen;
    size_t n = 0;

    p->pos++;
    gen_strbuf_init(&raw);
    while (p->pos < p->length) {
        unsigned char c = (unsigned char)p->src[p->pos];
        if (c == '"') {
            p->pos++;
            stolen = gen_strbuf_steal(&raw, &n);
            if (stolen == NULL) {
                yaml_fail(p, "out of memory");
                return GEN_ERR_OUT_OF_MEMORY;
            }
            if (!yaml_json_quote_append(out, stolen, n)) {
                gen_free(stolen);
                return GEN_ERR_OUT_OF_MEMORY;
            }
            gen_free(stolen);
            return GEN_OK;
        }
        if (c == '\\') {
            char e;
            p->pos++;
            if (p->pos >= p->length) {
                gen_strbuf_free(&raw);
                yaml_fail(p, "unterminated string escape");
                return GEN_ERR_SERIALIZATION;
            }
            e = p->src[p->pos++];
            if (e == 'n') {
                e = '\n';
            } else if (e == 't') {
                e = '\t';
            } else if (e == 'r') {
                e = '\r';
            } else if (e != '"' && e != '\\' && e != '/') {
                gen_strbuf_free(&raw);
                yaml_fail(p, "unsupported string escape");
                return GEN_ERR_SERIALIZATION;
            }
            if (!gen_strbuf_append_char(&raw, e)) {
                gen_strbuf_free(&raw);
                return GEN_ERR_OUT_OF_MEMORY;
            }
            continue;
        }
        if (c == '\n' || c == '\r') {
            gen_strbuf_free(&raw);
            yaml_fail(p, "unterminated quoted string");
            return GEN_ERR_SERIALIZATION;
        }
        if (!gen_strbuf_append_char(&raw, (char)c)) {
            gen_strbuf_free(&raw);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        p->pos++;
    }
    gen_strbuf_free(&raw);
    yaml_fail(p, "unterminated quoted string");
    return GEN_ERR_SERIALIZATION;
}

static GenResult yaml_parse_single_quoted(YamlParser *p, GenStrBuf *out)
{
    GenStrBuf raw;
    char *stolen;
    size_t n = 0;

    p->pos++;
    gen_strbuf_init(&raw);
    while (p->pos < p->length) {
        char c = p->src[p->pos];
        if (c == '\'') {
            p->pos++;
            if (p->pos < p->length && p->src[p->pos] == '\'') {
                if (!gen_strbuf_append_char(&raw, '\'')) {
                    gen_strbuf_free(&raw);
                    return GEN_ERR_OUT_OF_MEMORY;
                }
                p->pos++;
                continue;
            }
            stolen = gen_strbuf_steal(&raw, &n);
            if (stolen == NULL) {
                yaml_fail(p, "out of memory");
                return GEN_ERR_OUT_OF_MEMORY;
            }
            if (!yaml_json_quote_append(out, stolen, n)) {
                gen_free(stolen);
                return GEN_ERR_OUT_OF_MEMORY;
            }
            gen_free(stolen);
            return GEN_OK;
        }
        if (c == '\n' || c == '\r') {
            gen_strbuf_free(&raw);
            yaml_fail(p, "unterminated quoted string");
            return GEN_ERR_SERIALIZATION;
        }
        if (!gen_strbuf_append_char(&raw, c)) {
            gen_strbuf_free(&raw);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        p->pos++;
    }
    gen_strbuf_free(&raw);
    yaml_fail(p, "unterminated quoted string");
    return GEN_ERR_SERIALIZATION;
}

static bool yaml_is_plain_number(const char *s, size_t n, int *is_float)
{
    size_t i = 0;
    int saw_digit = 0;
    int saw_dot = 0;
    int saw_exp = 0;

    *is_float = 0;
    if (n == 0) {
        return false;
    }
    if (s[0] == '-' || s[0] == '+') {
        i = 1;
    }
    for (; i < n; i++) {
        char c = s[i];
        if (c >= '0' && c <= '9') {
            saw_digit = 1;
        } else if (c == '.' && !saw_dot && !saw_exp) {
            saw_dot = 1;
            *is_float = 1;
        } else if ((c == 'e' || c == 'E') && !saw_exp && saw_digit) {
            saw_exp = 1;
            *is_float = 1;
            if (i + 1u < n && (s[i + 1u] == '+' || s[i + 1u] == '-')) {
                i++;
            }
        } else {
            return false;
        }
    }
    return saw_digit != 0;
}

static GenResult yaml_emit_plain(GenStrBuf *out, const char *s, size_t n)
{
    int is_float = 0;

    if (n == 4 && (memcmp(s, "null", 4) == 0 || memcmp(s, "Null", 4) == 0 || memcmp(s, "NULL", 4) == 0)) {
        return gen_strbuf_append_cstr(out, "null") ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    }
    if (n == 1 && s[0] == '~') {
        return gen_strbuf_append_cstr(out, "null") ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    }
    if (n == 4 && (memcmp(s, "true", 4) == 0 || memcmp(s, "True", 4) == 0 || memcmp(s, "TRUE", 4) == 0)) {
        return gen_strbuf_append_cstr(out, "true") ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    }
    if (n == 5 &&
        (memcmp(s, "false", 5) == 0 || memcmp(s, "False", 5) == 0 || memcmp(s, "FALSE", 5) == 0)) {
        return gen_strbuf_append_cstr(out, "false") ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    }
    if (yaml_is_plain_number(s, n, &is_float)) {
        return gen_strbuf_append(out, s, n) ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    }
    return yaml_json_quote_append(out, s, n) ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
}

static GenResult yaml_parse_plain(YamlParser *p, GenStrBuf *out)
{
    size_t start = p->pos;
    size_t end;

    while (p->pos < p->length) {
        char c = p->src[p->pos];
        if (c == '\n' || c == '\r' || c == '#') {
            break;
        }
        p->pos++;
    }
    end = p->pos;
    while (end > start && (p->src[end - 1u] == ' ' || p->src[end - 1u] == '\t')) {
        end--;
    }
    if (end == start) {
        yaml_fail(p, "missing YAML value");
        return GEN_ERR_SERIALIZATION;
    }
    return yaml_emit_plain(out, p->src + start, end - start);
}

static GenResult yaml_parse_scalar(YamlParser *p, GenStrBuf *out)
{
    int c = yaml_peek(p);
    if (c == '"') {
        return yaml_parse_double_quoted(p, out);
    }
    if (c == '\'') {
        return yaml_parse_single_quoted(p, out);
    }
    if (c == '|' || c == '>') {
        yaml_fail(p, "YAML block scalars are not supported");
        return GEN_ERR_SERIALIZATION;
    }
    return yaml_parse_plain(p, out);
}

static bool yaml_at_key(const YamlParser *p)
{
    size_t i = p->pos;
    int in_quote = 0;
    char quote = 0;

    if (i >= p->length) {
        return false;
    }
    if (p->src[i] == '"' || p->src[i] == '\'') {
        quote = p->src[i];
        in_quote = 1;
        i++;
        while (i < p->length) {
            if (in_quote && p->src[i] == quote) {
                if (quote == '\'' && i + 1u < p->length && p->src[i + 1u] == '\'') {
                    i += 2;
                    continue;
                }
                i++;
                in_quote = 0;
                break;
            }
            if (p->src[i] == '\n' || p->src[i] == '\r') {
                return false;
            }
            i++;
        }
        while (i < p->length && (p->src[i] == ' ' || p->src[i] == '\t')) {
            i++;
        }
        return i < p->length && p->src[i] == ':';
    }
    while (i < p->length) {
        char c = p->src[i];
        if (c == ':') {
            if (i + 1u >= p->length) {
                return i > p->pos;
            }
            c = p->src[i + 1u];
            return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '#' || c == '\0';
        }
        if (c == '\n' || c == '\r' || c == '#' || c == ',') {
            return false;
        }
        i++;
    }
    return false;
}

static GenResult yaml_parse_key(YamlParser *p, GenStrBuf *out)
{
    int c = yaml_peek(p);
    GenResult rc;

    if (c == '"' || c == '\'') {
        rc = yaml_parse_scalar(p, out);
        if (rc != GEN_OK) {
            return rc;
        }
        yaml_skip_spaces(p);
        if (yaml_peek(p) != ':') {
            yaml_fail(p, "expected ':' after mapping key");
            return GEN_ERR_SERIALIZATION;
        }
        p->pos++;
        return GEN_OK;
    }
    {
        size_t start = p->pos;
        size_t end;
        while (p->pos < p->length && p->src[p->pos] != ':' && p->src[p->pos] != '\n' &&
               p->src[p->pos] != '\r') {
            p->pos++;
        }
        if (yaml_peek(p) != ':') {
            yaml_fail(p, "expected mapping key");
            return GEN_ERR_SERIALIZATION;
        }
        end = p->pos;
        while (end > start && (p->src[end - 1u] == ' ' || p->src[end - 1u] == '\t')) {
            end--;
        }
        if (end == start) {
            yaml_fail(p, "empty mapping key");
            return GEN_ERR_SERIALIZATION;
        }
        if (!yaml_json_quote_append(out, p->src + start, end - start)) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        p->pos++;
        return GEN_OK;
    }
}

static GenResult yaml_parse_mapping(YamlParser *p, GenStrBuf *out, size_t indent);
static GenResult yaml_parse_sequence(YamlParser *p, GenStrBuf *out, size_t indent);

static GenResult yaml_parse_after_colon(YamlParser *p, GenStrBuf *out, size_t key_indent)
{
    GenResult rc;
    size_t next_indent;

    yaml_skip_spaces(p);
    if (yaml_peek(p) == '#' || yaml_peek(p) == '\n' || yaml_peek(p) == '\r' || yaml_peek(p) < 0) {
        yaml_skip_to_eol(p);
        yaml_skip_blanks(p);
        if (p->pos >= p->length) {
            return gen_strbuf_append_cstr(out, "null") ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
        }
        next_indent = yaml_line_indent(p);
        if (next_indent <= key_indent) {
            return gen_strbuf_append_cstr(out, "null") ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
        }
        return yaml_parse_node(p, out, next_indent);
    }
    rc = yaml_parse_node(p, out, key_indent);
    yaml_skip_to_eol(p);
    return rc;
}

static GenResult yaml_parse_mapping(YamlParser *p, GenStrBuf *out, size_t indent)
{
    int first = 1;
    GenResult rc;

    GEN_UNUSED(indent);
    if (p->depth++ >= p->ctx->limits.max_nesting_depth) {
        yaml_fail(p, "YAML nesting too deep");
        return GEN_ERR_SERIALIZATION;
    }
    if (!gen_strbuf_append_char(out, '{')) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    for (;;) {
        yaml_skip_blanks(p);
        if (yaml_failed(p) || p->pos >= p->length) {
            break;
        }
        if (first) {
            indent = yaml_column(p);
        } else {
            if (yaml_line_indent(p) < indent) {
                break;
            }
            if (yaml_peek(p) == '-') {
                break;
            }
            if (yaml_line_indent(p) != indent) {
                yaml_fail(p, "invalid YAML mapping indentation");
                return GEN_ERR_SERIALIZATION;
            }
        }
        if (!yaml_at_key(p)) {
            yaml_fail(p, "expected mapping key");
            return GEN_ERR_SERIALIZATION;
        }
        if (!first && !gen_strbuf_append_char(out, ',')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        first = 0;
        rc = yaml_parse_key(p, out);
        if (rc != GEN_OK) {
            return rc;
        }
        if (!gen_strbuf_append_char(out, ':')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = yaml_parse_after_colon(p, out, indent);
        if (rc != GEN_OK) {
            return rc;
        }
    }
    p->depth--;
    return gen_strbuf_append_char(out, '}') ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
}

static GenResult yaml_parse_sequence(YamlParser *p, GenStrBuf *out, size_t indent)
{
    int first = 1;
    GenResult rc;

    if (p->depth++ >= p->ctx->limits.max_nesting_depth) {
        yaml_fail(p, "YAML nesting too deep");
        return GEN_ERR_SERIALIZATION;
    }
    if (!gen_strbuf_append_char(out, '[')) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    for (;;) {
        yaml_skip_blanks(p);
        if (yaml_failed(p) || p->pos >= p->length) {
            break;
        }
        if (yaml_line_indent(p) < indent) {
            break;
        }
        if (yaml_line_indent(p) != indent) {
            yaml_fail(p, "invalid YAML sequence indentation");
            return GEN_ERR_SERIALIZATION;
        }
        if (yaml_peek(p) != '-') {
            break;
        }
        p->pos++;
        if (yaml_peek(p) == ' ') {
            p->pos++;
        } else if (yaml_peek(p) != '\n' && yaml_peek(p) != '\r' && yaml_peek(p) != '#' &&
                   yaml_peek(p) >= 0) {
            yaml_fail(p, "expected space after '-'");
            return GEN_ERR_SERIALIZATION;
        }
        if (!first && !gen_strbuf_append_char(out, ',')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        first = 0;
        yaml_skip_spaces(p);
        if (yaml_peek(p) == '#' || yaml_peek(p) == '\n' || yaml_peek(p) == '\r' || yaml_peek(p) < 0) {
            size_t next_indent;
            yaml_skip_to_eol(p);
            yaml_skip_blanks(p);
            if (p->pos >= p->length || yaml_line_indent(p) <= indent) {
                if (!gen_strbuf_append_cstr(out, "null")) {
                    return GEN_ERR_OUT_OF_MEMORY;
                }
                continue;
            }
            next_indent = yaml_line_indent(p);
            rc = yaml_parse_node(p, out, next_indent);
            if (rc != GEN_OK) {
                return rc;
            }
            continue;
        }
        if (yaml_at_key(p)) {
            rc = yaml_parse_mapping(p, out, yaml_line_indent(p));
        } else {
            rc = yaml_parse_node(p, out, indent);
            yaml_skip_to_eol(p);
        }
        if (rc != GEN_OK) {
            return rc;
        }
    }
    p->depth--;
    return gen_strbuf_append_char(out, ']') ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
}

static GenResult yaml_parse_node(YamlParser *p, GenStrBuf *out, size_t min_indent)
{
    int c;
    GEN_UNUSED(min_indent);
    yaml_skip_spaces(p);
    if (yaml_failed(p)) {
        return GEN_ERR_SERIALIZATION;
    }
    c = yaml_peek(p);
    if (c < 0) {
        yaml_fail(p, "unexpected end of YAML");
        return GEN_ERR_SERIALIZATION;
    }
    if (c == '{' || c == '[') {
        return yaml_copy_flow(p, out);
    }
    if (c == '-') {
        char next = (p->pos + 1u < p->length) ? p->src[p->pos + 1u] : '\0';
        if (next == ' ' || next == '\n' || next == '\r' || next == '#' || next == '\0') {
            return yaml_parse_sequence(p, out, yaml_line_indent(p));
        }
    }
    if (yaml_at_key(p)) {
        return yaml_parse_mapping(p, out, yaml_line_indent(p));
    }
    return yaml_parse_scalar(p, out);
}

static void yaml_skip_document_marks(YamlParser *p)
{
    yaml_skip_blanks(p);
    if (yaml_starts_with(p, "---")) {
        char next;
        p->pos += 3;
        next = (p->pos < p->length) ? p->src[p->pos] : '\0';
        if (next == ' ' || next == '\t' || next == '\n' || next == '\r' || next == '#' ||
            next == '\0') {
            yaml_skip_to_eol(p);
        } else {
            p->pos -= 3;
        }
    }
    yaml_skip_blanks(p);
}

GenResult gen_document_from_yaml(GenContext *ctx, const char *yaml, GenDocument **out_document)
{
    YamlParser p;
    GenStrBuf json;
    char *converted = NULL;
    GenResult rc;

    if (ctx == NULL || yaml == NULL || out_document == NULL) {
        if (ctx != NULL) {
            gen_context_set_error(ctx, GEN_ERR_INVALID_ARGUMENT, 0, 0, 0, "invalid argument");
        }
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_document = NULL;
    gen_context_clear_error(ctx);
    memset(&p, 0, sizeof(p));
    p.ctx = ctx;
    p.src = yaml;
    p.length = strlen(yaml);
    if (p.length >= 3u && (unsigned char)p.src[0] == 0xefu && (unsigned char)p.src[1] == 0xbbu &&
        (unsigned char)p.src[2] == 0xbfu) {
        p.pos = 3;
    }
    yaml_skip_document_marks(&p);
    if (yaml_peek(&p) == '{') {
        return gen_document_from_json(ctx, p.src + p.pos, out_document);
    }
    gen_strbuf_init(&json);
    rc = yaml_parse_node(&p, &json, 0);
    if (rc != GEN_OK) {
        gen_strbuf_free(&json);
        return rc;
    }
    yaml_skip_blanks(&p);
    if (yaml_starts_with(&p, "...")) {
        p.pos += 3;
        yaml_skip_blanks(&p);
    }
    if (p.pos != p.length) {
        yaml_fail(&p, "trailing data after YAML document");
        gen_strbuf_free(&json);
        return GEN_ERR_SERIALIZATION;
    }
    converted = gen_strbuf_steal(&json, NULL);
    if (converted == NULL) {
        gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
        return GEN_ERR_OUT_OF_MEMORY;
    }
    rc = gen_document_from_json(ctx, converted, out_document);
    gen_free(converted);
    return rc;
}
