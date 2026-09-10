#include "serializer/binary.h"

#include "error/error.h"
#include "memory/allocator.h"

#define BIN_NULL_STRING ((uint32_t)0xffffffffu)

typedef struct {
    GenContext *ctx;
    const unsigned char *data;
    size_t length;
    size_t pos;
} BinReader;

static bool bin_host_little(void)
{
    const uint16_t one = 1u;
    return *(const unsigned char *)&one == 1u;
}

static bool bin_put_u8(GenStrBuf *buf, unsigned value)
{
    return gen_strbuf_append_char(buf, (char)(unsigned char)value);
}

static bool bin_put_u32(GenStrBuf *buf, uint32_t value)
{
    unsigned char b[4];
    b[0] = (unsigned char)(value & 0xffu);
    b[1] = (unsigned char)((value >> 8) & 0xffu);
    b[2] = (unsigned char)((value >> 16) & 0xffu);
    b[3] = (unsigned char)((value >> 24) & 0xffu);
    return gen_strbuf_append(buf, (const char *)b, 4);
}

static bool bin_put_u64(GenStrBuf *buf, uint64_t value)
{
    unsigned char b[8];
    int i;
    for (i = 0; i < 8; i++) {
        b[i] = (unsigned char)((value >> (8 * i)) & 0xffu);
    }
    return gen_strbuf_append(buf, (const char *)b, 8);
}

static bool bin_put_f64(GenStrBuf *buf, double value)
{
    unsigned char host[8];
    unsigned char le[8];
    int i;
    memcpy(host, &value, 8);
    if (bin_host_little()) {
        return gen_strbuf_append(buf, (const char *)host, 8);
    }
    for (i = 0; i < 8; i++) {
        le[i] = host[7 - i];
    }
    return gen_strbuf_append(buf, (const char *)le, 8);
}

static bool bin_put_cstr(GenStrBuf *buf, const char *s)
{
    size_t n;
    if (s == NULL) {
        return bin_put_u32(buf, BIN_NULL_STRING);
    }
    n = strlen(s);
    if (n > (size_t)UINT32_MAX) {
        return false;
    }
    return bin_put_u32(buf, (uint32_t)n) && gen_strbuf_append(buf, s, n);
}

static GenResult bin_put_value(const GenValue *value, GenStrBuf *buf);

static GenResult bin_put_value(const GenValue *value, GenStrBuf *buf)
{
    size_t i;

    if (value == NULL) {
        return bin_put_u8(buf, (unsigned)GEN_VALUE_NULL) ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    }
    if (!bin_put_u8(buf, (unsigned)value->kind)) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    switch (value->kind) {
    case GEN_VALUE_NULL:
        return GEN_OK;
    case GEN_VALUE_BOOL:
        return bin_put_u8(buf, value->u.boolean ? 1u : 0u) ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    case GEN_VALUE_INT:
        return bin_put_u64(buf, (uint64_t)value->u.integer) ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    case GEN_VALUE_FLOAT:
        return bin_put_f64(buf, value->u.floating) ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    case GEN_VALUE_STRING:
        if (value->u.string.length > (size_t)UINT32_MAX) {
            return GEN_ERR_SERIALIZATION;
        }
        if (!bin_put_u32(buf, (uint32_t)value->u.string.length) ||
            !gen_strbuf_append(buf, value->u.string.data, value->u.string.length)) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        return GEN_OK;
    case GEN_VALUE_REFERENCE:
        return bin_put_cstr(buf, value->u.reference.data) ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    case GEN_VALUE_LIST:
        if (value->u.list.count > (size_t)UINT32_MAX) {
            return GEN_ERR_SERIALIZATION;
        }
        if (!bin_put_u32(buf, (uint32_t)value->u.list.count)) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        for (i = 0; i < value->u.list.count; i++) {
            GenResult rc = bin_put_value(value->u.list.items[i], buf);
            if (rc != GEN_OK) {
                return rc;
            }
        }
        return GEN_OK;
    case GEN_VALUE_OBJECT:
        if (value->u.object.count > (size_t)UINT32_MAX) {
            return GEN_ERR_SERIALIZATION;
        }
        if (!bin_put_u32(buf, (uint32_t)value->u.object.count)) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        for (i = 0; i < value->u.object.count; i++) {
            if (!bin_put_cstr(buf, value->u.object.keys[i])) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
            {
                GenResult rc = bin_put_value(value->u.object.values[i], buf);
                if (rc != GEN_OK) {
                    return rc;
                }
            }
        }
        return GEN_OK;
    default:
        return GEN_ERR_TYPE;
    }
}

GenResult gen_document_to_binary(
    const GenDocument *document,
    char **out_bytes,
    size_t *out_length
)
{
    GenStrBuf buf;
    size_t i;
    GenResult rc;

    if (document == NULL || out_bytes == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_bytes = NULL;
    if (out_length != NULL) {
        *out_length = 0;
    }
    if (document->types_order.count > (size_t)UINT32_MAX ||
        document->sets_order.count > (size_t)UINT32_MAX ||
        document->entities_order.count > (size_t)UINT32_MAX ||
        document->memberships.count > (size_t)UINT32_MAX) {
        return GEN_ERR_SERIALIZATION;
    }
    gen_strbuf_init(&buf);
    if (!bin_put_u8(&buf, (unsigned)GEN_BINARY_MAGIC0) ||
        !bin_put_u8(&buf, (unsigned)GEN_BINARY_MAGIC1) ||
        !bin_put_u8(&buf, (unsigned)GEN_BINARY_MAGIC2) ||
        !bin_put_u8(&buf, (unsigned)GEN_BINARY_VERSION) ||
        !bin_put_u32(&buf, (uint32_t)document->types_order.count) ||
        !bin_put_u32(&buf, (uint32_t)document->sets_order.count) ||
        !bin_put_u32(&buf, (uint32_t)document->entities_order.count) ||
        !bin_put_u32(&buf, (uint32_t)document->memberships.count)) {
        gen_strbuf_free(&buf);
        return GEN_ERR_OUT_OF_MEMORY;
    }
    for (i = 0; i < document->types_order.count; i++) {
        GenTypeNode *t = (GenTypeNode *)document->types_order.items[i];
        unsigned kind = t->kind == GEN_TYPE_KIND_GENUS ? 0u : 1u;
        if (!bin_put_u8(&buf, kind) || !bin_put_cstr(&buf, t->name) ||
            !bin_put_cstr(&buf, t->parent != NULL ? t->parent->name : NULL)) {
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = bin_put_value(t->props, &buf);
        if (rc != GEN_OK) {
            gen_strbuf_free(&buf);
            return rc;
        }
    }
    for (i = 0; i < document->sets_order.count; i++) {
        GenSetNode *s = (GenSetNode *)document->sets_order.items[i];
        if (!bin_put_cstr(&buf, s->name)) {
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = bin_put_value(s->props, &buf);
        if (rc != GEN_OK) {
            gen_strbuf_free(&buf);
            return rc;
        }
    }
    for (i = 0; i < document->entities_order.count; i++) {
        GenEntity *e = (GenEntity *)document->entities_order.items[i];
        if (!bin_put_cstr(&buf, e->name) ||
            !bin_put_cstr(&buf, e->type != NULL ? e->type->name : NULL)) {
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = bin_put_value(e->value, &buf);
        if (rc != GEN_OK) {
            gen_strbuf_free(&buf);
            return rc;
        }
    }
    for (i = 0; i < document->memberships.count; i++) {
        GenMembershipRec *m = (GenMembershipRec *)document->memberships.items[i];
        if (!bin_put_cstr(&buf, m->entity) || !bin_put_cstr(&buf, m->set)) {
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }
    *out_bytes = gen_strbuf_steal(&buf, out_length);
    return *out_bytes != NULL ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
}

static void bin_fail(BinReader *r, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    gen_context_set_errorv(r->ctx, GEN_ERR_SERIALIZATION, 0, 0, r->pos, fmt, args);
    va_end(args);
}

static int bin_need(BinReader *r, size_t n)
{
    if (r->pos + n > r->length) {
        bin_fail(r, "truncated GenLang binary");
        return 0;
    }
    return 1;
}

static int bin_u8(BinReader *r, unsigned *out)
{
    if (!bin_need(r, 1)) {
        return 0;
    }
    *out = r->data[r->pos++];
    return 1;
}

static int bin_u32(BinReader *r, uint32_t *out)
{
    uint32_t v;
    if (!bin_need(r, 4)) {
        return 0;
    }
    v = (uint32_t)r->data[r->pos] | ((uint32_t)r->data[r->pos + 1u] << 8) |
        ((uint32_t)r->data[r->pos + 2u] << 16) | ((uint32_t)r->data[r->pos + 3u] << 24);
    r->pos += 4;
    *out = v;
    return 1;
}

static int bin_u64(BinReader *r, uint64_t *out)
{
    uint64_t v = 0;
    int i;
    if (!bin_need(r, 8)) {
        return 0;
    }
    for (i = 0; i < 8; i++) {
        v |= (uint64_t)r->data[r->pos++] << (8 * i);
    }
    *out = v;
    return 1;
}

static int bin_f64(BinReader *r, double *out)
{
    unsigned char host[8];
    int i;
    if (!bin_need(r, 8)) {
        return 0;
    }
    if (bin_host_little()) {
        memcpy(host, r->data + r->pos, 8);
    } else {
        for (i = 0; i < 8; i++) {
            host[i] = r->data[r->pos + (size_t)(7 - i)];
        }
    }
    r->pos += 8;
    memcpy(out, host, 8);
    return 1;
}

static bool bin_json_quote(GenStrBuf *buf, const char *data, size_t length)
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

static int bin_read_bytes(BinReader *r, uint32_t n, const char **out)
{
    if (n == BIN_NULL_STRING) {
        *out = NULL;
        return 1;
    }
    if (!bin_need(r, n)) {
        return 0;
    }
    *out = (const char *)(r->data + r->pos);
    r->pos += n;
    return 1;
}

static int bin_read_string(BinReader *r, const char **out, uint32_t *out_len)
{
    uint32_t n;
    if (!bin_u32(r, &n)) {
        return 0;
    }
    *out_len = n;
    return bin_read_bytes(r, n, out);
}

static GenResult bin_value_to_json(BinReader *r, GenStrBuf *json);

static GenResult bin_value_to_json(BinReader *r, GenStrBuf *json)
{
    unsigned tag = 0;
    uint32_t i;
    uint32_t count;

    if (!bin_u8(r, &tag)) {
        return GEN_ERR_SERIALIZATION;
    }
    switch (tag) {
    case GEN_VALUE_NULL:
        return gen_strbuf_append_cstr(json, "null") ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    case GEN_VALUE_BOOL: {
        unsigned b = 0;
        if (!bin_u8(r, &b)) {
            return GEN_ERR_SERIALIZATION;
        }
        return gen_strbuf_append_cstr(json, b ? "true" : "false") ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    }
    case GEN_VALUE_INT: {
        uint64_t bits = 0;
        if (!bin_u64(r, &bits)) {
            return GEN_ERR_SERIALIZATION;
        }
        return gen_strbuf_appendf(json, "%lld", (long long)(int64_t)bits) ? GEN_OK
                                                                          : GEN_ERR_OUT_OF_MEMORY;
    }
    case GEN_VALUE_FLOAT: {
        double d = 0;
        char tmp[64];
        int n;
        if (!bin_f64(r, &d)) {
            return GEN_ERR_SERIALIZATION;
        }
        n = snprintf(tmp, sizeof(tmp), "%.17g", d);
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
        return gen_strbuf_append_cstr(json, tmp) ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    }
    case GEN_VALUE_STRING: {
        const char *s = NULL;
        uint32_t n = 0;
        if (!bin_read_string(r, &s, &n) || s == NULL) {
            bin_fail(r, "invalid binary string");
            return GEN_ERR_SERIALIZATION;
        }
        return bin_json_quote(json, s, n) ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    }
    case GEN_VALUE_REFERENCE: {
        const char *s = NULL;
        uint32_t n = 0;
        if (!bin_read_string(r, &s, &n) || s == NULL) {
            bin_fail(r, "invalid binary reference");
            return GEN_ERR_SERIALIZATION;
        }
        if (!gen_strbuf_append_cstr(json, "{\"$ref\":") || !bin_json_quote(json, s, n) ||
            !gen_strbuf_append_char(json, '}')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        return GEN_OK;
    }
    case GEN_VALUE_LIST:
        if (!bin_u32(r, &count)) {
            return GEN_ERR_SERIALIZATION;
        }
        if (!gen_strbuf_append_char(json, '[')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        for (i = 0; i < count; i++) {
            GenResult rc;
            if (i > 0 && !gen_strbuf_append_char(json, ',')) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
            rc = bin_value_to_json(r, json);
            if (rc != GEN_OK) {
                return rc;
            }
        }
        return gen_strbuf_append_char(json, ']') ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    case GEN_VALUE_OBJECT:
        if (!bin_u32(r, &count) || !gen_strbuf_append_char(json, '{')) {
            return GEN_ERR_SERIALIZATION;
        }
        for (i = 0; i < count; i++) {
            const char *key = NULL;
            uint32_t n = 0;
            GenResult rc;
            if (i > 0 && !gen_strbuf_append_char(json, ',')) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
            if (!bin_read_string(r, &key, &n) || key == NULL) {
                bin_fail(r, "invalid binary object key");
                return GEN_ERR_SERIALIZATION;
            }
            if (!bin_json_quote(json, key, n) || !gen_strbuf_append_char(json, ':')) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
            rc = bin_value_to_json(r, json);
            if (rc != GEN_OK) {
                return rc;
            }
        }
        return gen_strbuf_append_char(json, '}') ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    default:
        bin_fail(r, "unknown binary value tag");
        return GEN_ERR_SERIALIZATION;
    }
}

static GenResult bin_optional_string_json(BinReader *r, GenStrBuf *json)
{
    const char *s = NULL;
    uint32_t n = 0;
    if (!bin_read_string(r, &s, &n)) {
        return GEN_ERR_SERIALIZATION;
    }
    if (s == NULL) {
        return gen_strbuf_append_cstr(json, "null") ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    }
    return bin_json_quote(json, s, n) ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
}

static GenResult bin_required_string_json(BinReader *r, GenStrBuf *json)
{
    const char *s = NULL;
    uint32_t n = 0;
    if (!bin_read_string(r, &s, &n) || s == NULL) {
        bin_fail(r, "expected binary string");
        return GEN_ERR_SERIALIZATION;
    }
    return bin_json_quote(json, s, n) ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
}

GenResult gen_document_from_binary(
    GenContext *ctx,
    const char *data,
    size_t length,
    GenDocument **out_document
)
{
    BinReader r;
    GenStrBuf json;
    char *converted = NULL;
    uint32_t ntypes = 0;
    uint32_t nsets = 0;
    uint32_t nents = 0;
    uint32_t nmemb = 0;
    uint32_t i;
    unsigned b0 = 0;
    unsigned b1 = 0;
    unsigned b2 = 0;
    unsigned ver = 0;
    GenResult rc;

    if (ctx == NULL || data == NULL || out_document == NULL) {
        if (ctx != NULL) {
            gen_context_set_error(ctx, GEN_ERR_INVALID_ARGUMENT, 0, 0, 0, "invalid argument");
        }
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_document = NULL;
    gen_context_clear_error(ctx);
    memset(&r, 0, sizeof(r));
    r.ctx = ctx;
    r.data = (const unsigned char *)data;
    r.length = length;
    if (!bin_u8(&r, &b0) || !bin_u8(&r, &b1) || !bin_u8(&r, &b2) || !bin_u8(&r, &ver)) {
        return GEN_ERR_SERIALIZATION;
    }
    if (b0 != (unsigned)GEN_BINARY_MAGIC0 || b1 != (unsigned)GEN_BINARY_MAGIC1 ||
        b2 != (unsigned)GEN_BINARY_MAGIC2) {
        bin_fail(&r, "not a GenLang binary document");
        return GEN_ERR_SERIALIZATION;
    }
    if (ver != (unsigned)GEN_BINARY_VERSION) {
        bin_fail(&r, "unsupported GenLang binary version");
        return GEN_ERR_SERIALIZATION;
    }
    if (!bin_u32(&r, &ntypes) || !bin_u32(&r, &nsets) || !bin_u32(&r, &nents) ||
        !bin_u32(&r, &nmemb)) {
        return GEN_ERR_SERIALIZATION;
    }
    gen_strbuf_init(&json);
    if (!gen_strbuf_append_cstr(&json, "{\"types\":[")) {
        gen_strbuf_free(&json);
        return GEN_ERR_OUT_OF_MEMORY;
    }
    for (i = 0; i < ntypes; i++) {
        unsigned kind = 0;
        if (i > 0 && !gen_strbuf_append_char(&json, ',')) {
            gen_strbuf_free(&json);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (!bin_u8(&r, &kind) || (kind != 0u && kind != 1u)) {
            bin_fail(&r, "invalid binary type kind");
            gen_strbuf_free(&json);
            return GEN_ERR_SERIALIZATION;
        }
        if (!gen_strbuf_append_cstr(&json, "{\"kind\":") ||
            !gen_strbuf_append_cstr(&json, kind == 0u ? "\"cins\"" : "\"tur\"") ||
            !gen_strbuf_append_cstr(&json, ",\"name\":")) {
            gen_strbuf_free(&json);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = bin_required_string_json(&r, &json);
        if (rc != GEN_OK) {
            gen_strbuf_free(&json);
            return rc;
        }
        if (!gen_strbuf_append_cstr(&json, ",\"parent\":")) {
            gen_strbuf_free(&json);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = bin_optional_string_json(&r, &json);
        if (rc != GEN_OK) {
            gen_strbuf_free(&json);
            return rc;
        }
        if (!gen_strbuf_append_cstr(&json, ",\"properties\":")) {
            gen_strbuf_free(&json);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = bin_value_to_json(&r, &json);
        if (rc != GEN_OK) {
            gen_strbuf_free(&json);
            return rc;
        }
        if (!gen_strbuf_append_char(&json, '}')) {
            gen_strbuf_free(&json);
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }
    if (!gen_strbuf_append_cstr(&json, "],\"sets\":[")) {
        gen_strbuf_free(&json);
        return GEN_ERR_OUT_OF_MEMORY;
    }
    for (i = 0; i < nsets; i++) {
        if (i > 0 && !gen_strbuf_append_char(&json, ',')) {
            gen_strbuf_free(&json);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (!gen_strbuf_append_cstr(&json, "{\"name\":")) {
            gen_strbuf_free(&json);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = bin_required_string_json(&r, &json);
        if (rc != GEN_OK) {
            gen_strbuf_free(&json);
            return rc;
        }
        if (!gen_strbuf_append_cstr(&json, ",\"properties\":")) {
            gen_strbuf_free(&json);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = bin_value_to_json(&r, &json);
        if (rc != GEN_OK) {
            gen_strbuf_free(&json);
            return rc;
        }
        if (!gen_strbuf_append_char(&json, '}')) {
            gen_strbuf_free(&json);
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }
    if (!gen_strbuf_append_cstr(&json, "],\"entities\":[")) {
        gen_strbuf_free(&json);
        return GEN_ERR_OUT_OF_MEMORY;
    }
    for (i = 0; i < nents; i++) {
        if (i > 0 && !gen_strbuf_append_char(&json, ',')) {
            gen_strbuf_free(&json);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (!gen_strbuf_append_cstr(&json, "{\"name\":")) {
            gen_strbuf_free(&json);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = bin_required_string_json(&r, &json);
        if (rc != GEN_OK) {
            gen_strbuf_free(&json);
            return rc;
        }
        if (!gen_strbuf_append_cstr(&json, ",\"type\":")) {
            gen_strbuf_free(&json);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = bin_optional_string_json(&r, &json);
        if (rc != GEN_OK) {
            gen_strbuf_free(&json);
            return rc;
        }
        if (!gen_strbuf_append_cstr(&json, ",\"value\":")) {
            gen_strbuf_free(&json);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = bin_value_to_json(&r, &json);
        if (rc != GEN_OK) {
            gen_strbuf_free(&json);
            return rc;
        }
        if (!gen_strbuf_append_char(&json, '}')) {
            gen_strbuf_free(&json);
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }
    if (!gen_strbuf_append_cstr(&json, "],\"memberships\":[")) {
        gen_strbuf_free(&json);
        return GEN_ERR_OUT_OF_MEMORY;
    }
    for (i = 0; i < nmemb; i++) {
        if (i > 0 && !gen_strbuf_append_char(&json, ',')) {
            gen_strbuf_free(&json);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (!gen_strbuf_append_cstr(&json, "{\"entity\":")) {
            gen_strbuf_free(&json);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = bin_required_string_json(&r, &json);
        if (rc != GEN_OK) {
            gen_strbuf_free(&json);
            return rc;
        }
        if (!gen_strbuf_append_cstr(&json, ",\"set\":")) {
            gen_strbuf_free(&json);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = bin_required_string_json(&r, &json);
        if (rc != GEN_OK) {
            gen_strbuf_free(&json);
            return rc;
        }
        if (!gen_strbuf_append_char(&json, '}')) {
            gen_strbuf_free(&json);
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }
    if (!gen_strbuf_append_cstr(&json, "]}")) {
        gen_strbuf_free(&json);
        return GEN_ERR_OUT_OF_MEMORY;
    }
    if (r.pos != r.length) {
        bin_fail(&r, "trailing data after GenLang binary");
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
