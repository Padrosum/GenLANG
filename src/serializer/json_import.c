#include "serializer/json.h"

#include "error/error.h"

typedef enum {
    JSON_NULL = 0,
    JSON_BOOL,
    JSON_INT,
    JSON_FLOAT,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonKind;

typedef struct Json Json;

struct Json {
    JsonKind kind;
    union {
        bool boolean;
        int64_t integer;
        double floating;
        struct {
            char *data;
            size_t length;
        } string;
        struct {
            Json **items;
            size_t count;
        } array;
        struct {
            char **keys;
            Json **values;
            size_t count;
        } object;
    } u;
};

typedef struct {
    GenContext *ctx;
    const char *src;
    size_t length;
    size_t pos;
    GenArena *arena;
} JsonParser;

static void json_fail(JsonParser *p, const char *fmt, ...)
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
    va_start(args, fmt);
    gen_context_set_errorv(p->ctx, GEN_ERR_SERIALIZATION, line, col, p->pos, fmt, args);
    va_end(args);
}

static void json_skip_ws(JsonParser *p)
{
    while (p->pos < p->length) {
        char c = p->src[p->pos];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
        p->pos++;
    }
}

static int json_peek(const JsonParser *p)
{
    if (p->pos >= p->length) {
        return -1;
    }
    return (unsigned char)p->src[p->pos];
}

static Json *json_new(JsonParser *p, JsonKind kind)
{
    Json *node = (Json *)gen_arena_alloc(p->arena, sizeof(Json));
    if (node == NULL) {
        json_fail(p, "out of memory");
        return NULL;
    }
    memset(node, 0, sizeof(Json));
    node->kind = kind;
    return node;
}

static Json *json_parse_value(JsonParser *p);

static char *json_parse_string_raw(JsonParser *p, size_t *out_len)
{
    GenStrBuf buf;
    json_skip_ws(p);
    if (json_peek(p) != '"') {
        json_fail(p, "expected string");
        return NULL;
    }
    p->pos++;
    gen_strbuf_init(&buf);
    while (p->pos < p->length) {
        unsigned char c = (unsigned char)p->src[p->pos];
        if (c == '"') {
            char *stolen;
            p->pos++;
            stolen = gen_strbuf_steal(&buf, out_len);
            if (stolen == NULL) {
                json_fail(p, "out of memory");
                return NULL;
            }
            {
                char *arena_s = gen_arena_strndup(p->arena, stolen, *out_len);
                gen_free(stolen);
                if (arena_s == NULL) {
                    json_fail(p, "out of memory");
                    return NULL;
                }
                return arena_s;
            }
        }
        if (c == '\\') {
            char e;
            p->pos++;
            if (p->pos >= p->length) {
                gen_strbuf_free(&buf);
                json_fail(p, "unterminated string escape");
                return NULL;
            }
            e = p->src[p->pos++];
            if (e == '"' || e == '\\' || e == '/') {
                if (!gen_strbuf_append_char(&buf, e)) {
                    gen_strbuf_free(&buf);
                    json_fail(p, "out of memory");
                    return NULL;
                }
            } else if (e == 'n') {
                if (!gen_strbuf_append_char(&buf, '\n')) {
                    gen_strbuf_free(&buf);
                    json_fail(p, "out of memory");
                    return NULL;
                }
            } else if (e == 't') {
                if (!gen_strbuf_append_char(&buf, '\t')) {
                    gen_strbuf_free(&buf);
                    json_fail(p, "out of memory");
                    return NULL;
                }
            } else if (e == 'r') {
                if (!gen_strbuf_append_char(&buf, '\r')) {
                    gen_strbuf_free(&buf);
                    json_fail(p, "out of memory");
                    return NULL;
                }
            } else if (e == 'u') {
                unsigned int cp = 0;
                int hi;
                if (p->pos + 4u > p->length) {
                    gen_strbuf_free(&buf);
                    json_fail(p, "invalid unicode escape");
                    return NULL;
                }
                for (hi = 0; hi < 4; hi++) {
                    char h = p->src[p->pos++];
                    cp <<= 4;
                    if (h >= '0' && h <= '9') {
                        cp |= (unsigned int)(h - '0');
                    } else if (h >= 'a' && h <= 'f') {
                        cp |= (unsigned int)(h - 'a' + 10);
                    } else if (h >= 'A' && h <= 'F') {
                        cp |= (unsigned int)(h - 'A' + 10);
                    } else {
                        gen_strbuf_free(&buf);
                        json_fail(p, "invalid unicode escape");
                        return NULL;
                    }
                }
                if (cp < 0x80u) {
                    if (!gen_strbuf_append_char(&buf, (char)cp)) {
                        gen_strbuf_free(&buf);
                        json_fail(p, "out of memory");
                        return NULL;
                    }
                } else {
                    gen_strbuf_free(&buf);
                    json_fail(p, "unicode escape above U+007F is not supported in JSON import");
                    return NULL;
                }
            } else {
                gen_strbuf_free(&buf);
                json_fail(p, "invalid string escape");
                return NULL;
            }
            continue;
        }
        if (c < 0x20u) {
            gen_strbuf_free(&buf);
            json_fail(p, "unescaped control character in string");
            return NULL;
        }
        if (!gen_strbuf_append_char(&buf, (char)c)) {
            gen_strbuf_free(&buf);
            json_fail(p, "out of memory");
            return NULL;
        }
        p->pos++;
    }
    gen_strbuf_free(&buf);
    json_fail(p, "unterminated string");
    return NULL;
}

static Json *json_parse_string(JsonParser *p)
{
    Json *node = json_new(p, JSON_STRING);
    if (node == NULL) {
        return NULL;
    }
    node->u.string.data = json_parse_string_raw(p, &node->u.string.length);
    if (node->u.string.data == NULL) {
        return NULL;
    }
    return node;
}

static Json *json_parse_number(JsonParser *p)
{
    Json *node;
    size_t start = p->pos;
    int is_float = 0;
    char *tmp;
    char *end = NULL;

    if (json_peek(p) == '-') {
        p->pos++;
    }
    if (json_peek(p) < '0' || json_peek(p) > '9') {
        json_fail(p, "invalid number");
        return NULL;
    }
    if (json_peek(p) == '0') {
        p->pos++;
    } else {
        while (json_peek(p) >= '0' && json_peek(p) <= '9') {
            p->pos++;
        }
    }
    if (json_peek(p) == '.') {
        is_float = 1;
        p->pos++;
        if (json_peek(p) < '0' || json_peek(p) > '9') {
            json_fail(p, "invalid number");
            return NULL;
        }
        while (json_peek(p) >= '0' && json_peek(p) <= '9') {
            p->pos++;
        }
    }
    if (json_peek(p) == 'e' || json_peek(p) == 'E') {
        is_float = 1;
        p->pos++;
        if (json_peek(p) == '+' || json_peek(p) == '-') {
            p->pos++;
        }
        if (json_peek(p) < '0' || json_peek(p) > '9') {
            json_fail(p, "invalid number");
            return NULL;
        }
        while (json_peek(p) >= '0' && json_peek(p) <= '9') {
            p->pos++;
        }
    }
    tmp = gen_strndup(p->src + start, p->pos - start);
    if (tmp == NULL) {
        json_fail(p, "out of memory");
        return NULL;
    }
    node = json_new(p, is_float ? JSON_FLOAT : JSON_INT);
    if (node == NULL) {
        gen_free(tmp);
        return NULL;
    }
    errno = 0;
    if (is_float) {
        node->u.floating = strtod(tmp, &end);
    } else {
        node->u.integer = strtoll(tmp, &end, 10);
    }
    gen_free(tmp);
    if (errno == ERANGE) {
        json_fail(p, "number out of range");
        return NULL;
    }
    return node;
}

static Json *json_parse_array(JsonParser *p)
{
    Json *node = json_new(p, JSON_ARRAY);
    Json **items = NULL;
    size_t count = 0;
    size_t cap = 0;

    p->pos++; /* [ */
    json_skip_ws(p);
    if (json_peek(p) == ']') {
        p->pos++;
        return node;
    }
    for (;;) {
        Json *item;
        Json **grown;
        item = json_parse_value(p);
        if (item == NULL) {
            return NULL;
        }
        if (count + 1u > cap) {
            cap = cap == 0 ? 4u : cap * 2u;
            grown = (Json **)gen_arena_alloc(p->arena, cap * sizeof(Json *));
            if (grown == NULL) {
                json_fail(p, "out of memory");
                return NULL;
            }
            if (items != NULL && count > 0) {
                memcpy(grown, items, count * sizeof(Json *));
            }
            items = grown;
        }
        items[count++] = item;
        json_skip_ws(p);
        if (json_peek(p) == ',') {
            p->pos++;
            json_skip_ws(p);
            continue;
        }
        if (json_peek(p) == ']') {
            p->pos++;
            break;
        }
        json_fail(p, "expected ',' or ']' in array");
        return NULL;
    }
    node->u.array.items = items;
    node->u.array.count = count;
    return node;
}

static Json *json_parse_object(JsonParser *p)
{
    Json *node = json_new(p, JSON_OBJECT);
    char **keys = NULL;
    Json **vals = NULL;
    size_t count = 0;
    size_t cap = 0;

    p->pos++; /* { */
    json_skip_ws(p);
    if (json_peek(p) == '}') {
        p->pos++;
        return node;
    }
    for (;;) {
        char *key;
        Json *val;
        size_t key_len = 0;
        char **nk;
        Json **nv;

        json_skip_ws(p);
        key = json_parse_string_raw(p, &key_len);
        if (key == NULL) {
            return NULL;
        }
        json_skip_ws(p);
        if (json_peek(p) != ':') {
            json_fail(p, "expected ':' after object key");
            return NULL;
        }
        p->pos++;
        val = json_parse_value(p);
        if (val == NULL) {
            return NULL;
        }
        if (count + 1u > cap) {
            cap = cap == 0 ? 4u : cap * 2u;
            nk = (char **)gen_arena_alloc(p->arena, cap * sizeof(char *));
            nv = (Json **)gen_arena_alloc(p->arena, cap * sizeof(Json *));
            if (nk == NULL || nv == NULL) {
                json_fail(p, "out of memory");
                return NULL;
            }
            if (keys != NULL && count > 0) {
                memcpy(nk, keys, count * sizeof(char *));
                memcpy(nv, vals, count * sizeof(Json *));
            }
            keys = nk;
            vals = nv;
        }
        keys[count] = key;
        vals[count] = val;
        count++;
        json_skip_ws(p);
        if (json_peek(p) == ',') {
            p->pos++;
            continue;
        }
        if (json_peek(p) == '}') {
            p->pos++;
            break;
        }
        json_fail(p, "expected ',' or '}' in object");
        return NULL;
    }
    node->u.object.keys = keys;
    node->u.object.values = vals;
    node->u.object.count = count;
    return node;
}

static int json_starts_with(JsonParser *p, const char *word)
{
    size_t n = strlen(word);
    if (p->pos + n > p->length) {
        return 0;
    }
    return memcmp(p->src + p->pos, word, n) == 0;
}

static Json *json_parse_value(JsonParser *p)
{
    int c;
    json_skip_ws(p);
    c = json_peek(p);
    if (c < 0) {
        json_fail(p, "unexpected end of JSON");
        return NULL;
    }
    if (c == '"') {
        return json_parse_string(p);
    }
    if (c == '{') {
        return json_parse_object(p);
    }
    if (c == '[') {
        return json_parse_array(p);
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
        return json_parse_number(p);
    }
    if (json_starts_with(p, "true")) {
        Json *node = json_new(p, JSON_BOOL);
        if (node == NULL) {
            return NULL;
        }
        node->u.boolean = true;
        p->pos += 4;
        return node;
    }
    if (json_starts_with(p, "false")) {
        Json *node = json_new(p, JSON_BOOL);
        if (node == NULL) {
            return NULL;
        }
        node->u.boolean = false;
        p->pos += 5;
        return node;
    }
    if (json_starts_with(p, "null")) {
        Json *node = json_new(p, JSON_NULL);
        p->pos += 4;
        return node;
    }
    json_fail(p, "unexpected JSON token");
    return NULL;
}

static const Json *json_obj_get(const Json *obj, const char *key)
{
    size_t i;
    if (obj == NULL || obj->kind != JSON_OBJECT) {
        return NULL;
    }
    for (i = 0; i < obj->u.object.count; i++) {
        if (strcmp(obj->u.object.keys[i], key) == 0) {
            return obj->u.object.values[i];
        }
    }
    return NULL;
}

static const char *json_as_string(const Json *n)
{
    if (n == NULL || n->kind != JSON_STRING) {
        return NULL;
    }
    return n->u.string.data;
}

static int json_is_null(const Json *n)
{
    return n != NULL && n->kind == JSON_NULL;
}

static bool json_ident_ok(const char *s)
{
    size_t n;
    size_t i = 0;
    uint32_t cp;
    size_t sz;

    if (s == NULL || s[0] == '\0') {
        return false;
    }
    n = strlen(s);
    if (!gen_utf8_next(s, n, 0, &cp, &sz) || !gen_utf8_is_ident_start(cp)) {
        return false;
    }
    i = sz;
    while (i < n) {
        if (!gen_utf8_next(s, n, i, &cp, &sz) || !gen_utf8_is_ident_continue(cp)) {
            return false;
        }
        i += sz;
    }
    return true;
}

static bool gl_escape_append(GenStrBuf *buf, const char *data, size_t length)
{
    size_t i;
    if (!gen_strbuf_append_char(buf, '"')) {
        return false;
    }
    for (i = 0; i < length; i++) {
        char c = data[i];
        if (c == '"' || c == '\\') {
            if (!gen_strbuf_append_char(buf, '\\') || !gen_strbuf_append_char(buf, c)) {
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
        } else if (!gen_strbuf_append_char(buf, c)) {
            return false;
        }
    }
    return gen_strbuf_append_char(buf, '"');
}

static GenResult json_emit_value(GenContext *ctx, const Json *node, GenStrBuf *buf)
{
    size_t i;
    GenResult rc;

    if (node == NULL) {
        gen_context_set_error(ctx, GEN_ERR_SERIALIZATION, 0, 0, 0, "missing JSON value");
        return GEN_ERR_SERIALIZATION;
    }
    switch (node->kind) {
    case JSON_NULL:
        return gen_strbuf_append_cstr(buf, "null") ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    case JSON_BOOL:
        return gen_strbuf_append_cstr(buf, node->u.boolean ? "true" : "false")
                   ? GEN_OK
                   : GEN_ERR_OUT_OF_MEMORY;
    case JSON_INT:
        return gen_strbuf_appendf(buf, "%lld", (long long)node->u.integer) ? GEN_OK
                                                                          : GEN_ERR_OUT_OF_MEMORY;
    case JSON_FLOAT: {
        char tmp[64];
        int n = snprintf(tmp, sizeof(tmp), "%.17g", node->u.floating);
        if (n < 0) {
            return GEN_ERR_SERIALIZATION;
        }
        if (strchr(tmp, '.') == NULL && strchr(tmp, 'e') == NULL && strchr(tmp, 'E') == NULL) {
            if (n + 2 < (int)sizeof(tmp)) {
                tmp[n] = '.';
                tmp[n + 1] = '0';
                tmp[n + 2] = '\0';
            }
        }
        return gen_strbuf_append_cstr(buf, tmp) ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    }
    case JSON_STRING:
        return gl_escape_append(buf, node->u.string.data, node->u.string.length)
                   ? GEN_OK
                   : GEN_ERR_OUT_OF_MEMORY;
    case JSON_ARRAY:
        if (!gen_strbuf_append_char(buf, '[')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        for (i = 0; i < node->u.array.count; i++) {
            if (i > 0 && !gen_strbuf_append_cstr(buf, ", ")) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
            rc = json_emit_value(ctx, node->u.array.items[i], buf);
            if (rc != GEN_OK) {
                return rc;
            }
        }
        return gen_strbuf_append_char(buf, ']') ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    case JSON_OBJECT:
        if (node->u.object.count == 1u && strcmp(node->u.object.keys[0], "$ref") == 0) {
            const char *ref = json_as_string(node->u.object.values[0]);
            if (ref == NULL || !json_ident_ok(ref)) {
                gen_context_set_error(
                    ctx, GEN_ERR_SERIALIZATION, 0, 0, 0, "invalid JSON reference"
                );
                return GEN_ERR_SERIALIZATION;
            }
            return gen_strbuf_appendf(buf, "@%s", ref) ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
        }
        if (node->u.object.count == 0) {
            return gen_strbuf_append_cstr(buf, "{}") ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
        }
        if (!gen_strbuf_append_cstr(buf, "{\n")) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        for (i = 0; i < node->u.object.count; i++) {
            if (!json_ident_ok(node->u.object.keys[i])) {
                gen_context_set_error(
                    ctx,
                    GEN_ERR_SERIALIZATION,
                    0,
                    0,
                    0,
                    "JSON object key '%s' is not a GenLang identifier",
                    node->u.object.keys[i]
                );
                return GEN_ERR_SERIALIZATION;
            }
            if (!gen_strbuf_appendf(buf, "    %s = ", node->u.object.keys[i])) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
            rc = json_emit_value(ctx, node->u.object.values[i], buf);
            if (rc != GEN_OK) {
                return rc;
            }
            if (!gen_strbuf_append_char(buf, '\n')) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
        }
        return gen_strbuf_append_char(buf, '}') ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    default:
        return GEN_ERR_SERIALIZATION;
    }
}

static GenResult json_emit_document(GenContext *ctx, const Json *root, GenStrBuf *buf)
{
    const Json *types;
    const Json *sets;
    const Json *entities;
    const Json *memberships;
    size_t i;

    if (root == NULL || root->kind != JSON_OBJECT) {
        gen_context_set_error(ctx, GEN_ERR_SERIALIZATION, 0, 0, 0, "JSON document must be an object");
        return GEN_ERR_SERIALIZATION;
    }
    types = json_obj_get(root, "types");
    sets = json_obj_get(root, "sets");
    entities = json_obj_get(root, "entities");
    memberships = json_obj_get(root, "memberships");
    if (types == NULL || types->kind != JSON_ARRAY || sets == NULL || sets->kind != JSON_ARRAY ||
        entities == NULL || entities->kind != JSON_ARRAY || memberships == NULL ||
        memberships->kind != JSON_ARRAY) {
        gen_context_set_error(
            ctx,
            GEN_ERR_SERIALIZATION,
            0,
            0,
            0,
            "JSON must contain types, sets, entities, and memberships arrays"
        );
        return GEN_ERR_SERIALIZATION;
    }

    for (i = 0; i < types->u.array.count; i++) {
        const Json *t = types->u.array.items[i];
        const char *kind;
        const char *name;
        const Json *parent;
        if (t->kind != JSON_OBJECT) {
            gen_context_set_error(ctx, GEN_ERR_SERIALIZATION, 0, 0, 0, "type entry must be an object");
            return GEN_ERR_SERIALIZATION;
        }
        kind = json_as_string(json_obj_get(t, "kind"));
        name = json_as_string(json_obj_get(t, "name"));
        parent = json_obj_get(t, "parent");
        if (kind == NULL || (strcmp(kind, "cins") != 0 && strcmp(kind, "tur") != 0) ||
            name == NULL || !json_ident_ok(name)) {
            gen_context_set_error(ctx, GEN_ERR_SERIALIZATION, 0, 0, 0, "invalid type entry");
            return GEN_ERR_SERIALIZATION;
        }
        if (parent != NULL && !json_is_null(parent) && json_as_string(parent) != NULL) {
            if (!json_ident_ok(json_as_string(parent))) {
                gen_context_set_error(ctx, GEN_ERR_SERIALIZATION, 0, 0, 0, "invalid parent name");
                return GEN_ERR_SERIALIZATION;
            }
            if (!gen_strbuf_appendf(buf, "%s %s -> %s", kind, name, json_as_string(parent))) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
        } else if (!gen_strbuf_appendf(buf, "%s %s", kind, name)) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        {
            const Json *props = json_obj_get(t, "properties");
            if (props != NULL && !json_is_null(props)) {
                GenResult rc;
                if (!gen_strbuf_append_char(buf, ' ')) {
                    return GEN_ERR_OUT_OF_MEMORY;
                }
                rc = json_emit_value(ctx, props, buf);
                if (rc != GEN_OK) {
                    return rc;
                }
            }
            if (!gen_strbuf_append_char(buf, '\n')) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
        }
    }
    if (types->u.array.count > 0 && !gen_strbuf_append_char(buf, '\n')) {
        return GEN_ERR_OUT_OF_MEMORY;
    }

    for (i = 0; i < sets->u.array.count; i++) {
        const Json *s = sets->u.array.items[i];
        const char *name;
        if (s->kind != JSON_OBJECT) {
            gen_context_set_error(ctx, GEN_ERR_SERIALIZATION, 0, 0, 0, "set entry must be an object");
            return GEN_ERR_SERIALIZATION;
        }
        name = json_as_string(json_obj_get(s, "name"));
        if (name == NULL || !json_ident_ok(name)) {
            gen_context_set_error(ctx, GEN_ERR_SERIALIZATION, 0, 0, 0, "invalid set entry");
            return GEN_ERR_SERIALIZATION;
        }
        if (!gen_strbuf_appendf(buf, "kume %s", name)) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        {
            const Json *props = json_obj_get(s, "properties");
            if (props != NULL && !json_is_null(props)) {
                GenResult rc;
                if (!gen_strbuf_append_char(buf, ' ')) {
                    return GEN_ERR_OUT_OF_MEMORY;
                }
                rc = json_emit_value(ctx, props, buf);
                if (rc != GEN_OK) {
                    return rc;
                }
            }
            if (!gen_strbuf_append_char(buf, '\n')) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
        }
    }
    if (sets->u.array.count > 0 && !gen_strbuf_append_char(buf, '\n')) {
        return GEN_ERR_OUT_OF_MEMORY;
    }

    for (i = 0; i < entities->u.array.count; i++) {
        const Json *e = entities->u.array.items[i];
        const char *name;
        const Json *type;
        const Json *value;
        GenResult rc;
        if (e->kind != JSON_OBJECT) {
            gen_context_set_error(
                ctx, GEN_ERR_SERIALIZATION, 0, 0, 0, "entity entry must be an object"
            );
            return GEN_ERR_SERIALIZATION;
        }
        name = json_as_string(json_obj_get(e, "name"));
        type = json_obj_get(e, "type");
        value = json_obj_get(e, "value");
        if (name == NULL || !json_ident_ok(name) || value == NULL) {
            gen_context_set_error(ctx, GEN_ERR_SERIALIZATION, 0, 0, 0, "invalid entity entry");
            return GEN_ERR_SERIALIZATION;
        }
        if (type != NULL && !json_is_null(type) && json_as_string(type) != NULL) {
            if (!json_ident_ok(json_as_string(type))) {
                gen_context_set_error(ctx, GEN_ERR_SERIALIZATION, 0, 0, 0, "invalid entity type");
                return GEN_ERR_SERIALIZATION;
            }
            if (!gen_strbuf_appendf(buf, "veri %s : %s ", name, json_as_string(type))) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
        } else if (value->kind == JSON_OBJECT) {
            if (!gen_strbuf_appendf(buf, "veri %s ", name)) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
        } else if (!gen_strbuf_appendf(buf, "veri %s = ", name)) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = json_emit_value(ctx, value, buf);
        if (rc != GEN_OK) {
            return rc;
        }
        if (!gen_strbuf_append_cstr(buf, "\n\n")) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }

    for (i = 0; i < memberships->u.array.count; i++) {
        const Json *m = memberships->u.array.items[i];
        const char *entity;
        const char *set;
        if (m->kind != JSON_OBJECT) {
            gen_context_set_error(
                ctx, GEN_ERR_SERIALIZATION, 0, 0, 0, "membership entry must be an object"
            );
            return GEN_ERR_SERIALIZATION;
        }
        entity = json_as_string(json_obj_get(m, "entity"));
        set = json_as_string(json_obj_get(m, "set"));
        if (entity == NULL || set == NULL || !json_ident_ok(entity) || !json_ident_ok(set)) {
            gen_context_set_error(ctx, GEN_ERR_SERIALIZATION, 0, 0, 0, "invalid membership entry");
            return GEN_ERR_SERIALIZATION;
        }
        if (!gen_strbuf_appendf(buf, "uye %s -> %s\n", entity, set)) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }
    return GEN_OK;
}

GenResult gen_document_from_json(
    GenContext *ctx,
    const char *json,
    GenDocument **out_document
)
{
    JsonParser p;
    Json *root;
    GenStrBuf gl;
    char *source = NULL;
    GenResult rc;

    if (ctx == NULL || json == NULL || out_document == NULL) {
        if (ctx != NULL) {
            gen_context_set_error(ctx, GEN_ERR_INVALID_ARGUMENT, 0, 0, 0, "invalid argument");
        }
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_document = NULL;
    gen_context_clear_error(ctx);
    memset(&p, 0, sizeof(p));
    p.ctx = ctx;
    p.src = json;
    p.length = strlen(json);
    p.arena = gen_arena_create();
    if (p.arena == NULL) {
        gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
        return GEN_ERR_OUT_OF_MEMORY;
    }
    root = json_parse_value(&p);
    if (root == NULL) {
        rc = ctx->last_error.code != GEN_OK ? ctx->last_error.code : GEN_ERR_SERIALIZATION;
        gen_arena_destroy(p.arena);
        return rc;
    }
    json_skip_ws(&p);
    if (p.pos != p.length) {
        json_fail(&p, "trailing data after JSON value");
        gen_arena_destroy(p.arena);
        return GEN_ERR_SERIALIZATION;
    }
    gen_strbuf_init(&gl);
    rc = json_emit_document(ctx, root, &gl);
    gen_arena_destroy(p.arena);
    if (rc != GEN_OK) {
        gen_strbuf_free(&gl);
        return rc;
    }
    source = gen_strbuf_steal(&gl, NULL);
    if (source == NULL) {
        gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
        return GEN_ERR_OUT_OF_MEMORY;
    }
    rc = gen_document_parse(ctx, source, out_document);
    gen_free(source);
    return rc;
}
