#include "serializer/json.h"

static bool gen_json_append_quoted(GenStrBuf *buf, const char *data, size_t length)
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
        } else {
            if (!gen_strbuf_append_char(buf, (char)c)) {
                return false;
            }
        }
    }
    return gen_strbuf_append_char(buf, '"');
}

static bool gen_json_quote_cstr(GenStrBuf *buf, const char *s)
{
    if (s == NULL) {
        return gen_strbuf_append_cstr(buf, "null");
    }
    return gen_json_append_quoted(buf, s, strlen(s));
}

GenResult gen_value_to_json_impl(const GenValue *value, GenStrBuf *buf)
{
    size_t i;
    GenResult rc;

    if (value == NULL || buf == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    switch (value->kind) {
    case GEN_VALUE_NULL:
        return gen_strbuf_append_cstr(buf, "null") ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    case GEN_VALUE_BOOL:
        return gen_strbuf_append_cstr(buf, value->u.boolean ? "true" : "false")
                   ? GEN_OK
                   : GEN_ERR_OUT_OF_MEMORY;
    case GEN_VALUE_INT:
        return gen_strbuf_appendf(buf, "%lld", (long long)value->u.integer)
                   ? GEN_OK
                   : GEN_ERR_OUT_OF_MEMORY;
    case GEN_VALUE_FLOAT: {
        char tmp[64];
        int n = snprintf(tmp, sizeof(tmp), "%.17g", value->u.floating);
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
    case GEN_VALUE_STRING:
        return gen_json_append_quoted(buf, value->u.string.data, value->u.string.length)
                   ? GEN_OK
                   : GEN_ERR_OUT_OF_MEMORY;
    case GEN_VALUE_REFERENCE:
        if (!gen_strbuf_append_cstr(buf, "{\"$ref\":") ||
            !gen_json_quote_cstr(buf, value->u.reference.data) ||
            !gen_strbuf_append_char(buf, '}')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        return GEN_OK;
    case GEN_VALUE_LIST:
        if (!gen_strbuf_append_char(buf, '[')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        for (i = 0; i < value->u.list.count; i++) {
            if (i > 0 && !gen_strbuf_append_char(buf, ',')) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
            rc = gen_value_to_json_impl(value->u.list.items[i], buf);
            if (rc != GEN_OK) {
                return rc;
            }
        }
        return gen_strbuf_append_char(buf, ']') ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    case GEN_VALUE_OBJECT:
        if (!gen_strbuf_append_char(buf, '{')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        for (i = 0; i < value->u.object.count; i++) {
            if (i > 0 && !gen_strbuf_append_char(buf, ',')) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
            if (!gen_json_quote_cstr(buf, value->u.object.keys[i]) ||
                !gen_strbuf_append_char(buf, ':')) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
            rc = gen_value_to_json_impl(value->u.object.values[i], buf);
            if (rc != GEN_OK) {
                return rc;
            }
        }
        return gen_strbuf_append_char(buf, '}') ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    default:
        return GEN_ERR_TYPE;
    }
}

GenResult gen_value_to_json(const GenValue *value, char **out_text, size_t *out_length)
{
    GenStrBuf buf;
    GenResult rc;

    if (value == NULL || out_text == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_text = NULL;
    gen_strbuf_init(&buf);
    rc = gen_value_to_json_impl(value, &buf);
    if (rc != GEN_OK) {
        gen_strbuf_free(&buf);
        return rc;
    }
    *out_text = gen_strbuf_steal(&buf, out_length);
    return *out_text != NULL ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
}

GenResult gen_document_to_json_impl(const GenDocument *document, GenStrBuf *buf)
{
    size_t i;

    if (!gen_strbuf_append_cstr(buf, "{\"types\":[")) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    for (i = 0; i < document->types_order.count; i++) {
        GenTypeNode *t = (GenTypeNode *)document->types_order.items[i];
        const char *kind = t->kind == GEN_TYPE_KIND_GENUS ? "cins" : "tur";
        if (i > 0 && !gen_strbuf_append_char(buf, ',')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (!gen_strbuf_append_cstr(buf, "{\"kind\":") || !gen_json_quote_cstr(buf, kind) ||
            !gen_strbuf_append_cstr(buf, ",\"name\":") || !gen_json_quote_cstr(buf, t->name) ||
            !gen_strbuf_append_cstr(buf, ",\"parent\":") ||
            !gen_json_quote_cstr(buf, t->parent != NULL ? t->parent->name : NULL) ||
            !gen_strbuf_append_cstr(buf, ",\"properties\":")) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (t->props != NULL) {
            GenResult rc = gen_value_to_json_impl(t->props, buf);
            if (rc != GEN_OK) {
                return rc;
            }
        } else if (!gen_strbuf_append_cstr(buf, "null")) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (!gen_strbuf_append_char(buf, '}')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }
    if (!gen_strbuf_append_cstr(buf, "],\"sets\":[")) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    for (i = 0; i < document->sets_order.count; i++) {
        GenSetNode *s = (GenSetNode *)document->sets_order.items[i];
        if (i > 0 && !gen_strbuf_append_char(buf, ',')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (!gen_strbuf_append_cstr(buf, "{\"name\":") || !gen_json_quote_cstr(buf, s->name) ||
            !gen_strbuf_append_cstr(buf, ",\"properties\":")) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (s->props != NULL) {
            GenResult rc = gen_value_to_json_impl(s->props, buf);
            if (rc != GEN_OK) {
                return rc;
            }
        } else if (!gen_strbuf_append_cstr(buf, "null")) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (!gen_strbuf_append_char(buf, '}')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }
    if (!gen_strbuf_append_cstr(buf, "],\"entities\":[")) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    for (i = 0; i < document->entities_order.count; i++) {
        GenEntity *e = (GenEntity *)document->entities_order.items[i];
        GenResult rc;
        if (i > 0 && !gen_strbuf_append_char(buf, ',')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (!gen_strbuf_append_cstr(buf, "{\"name\":") || !gen_json_quote_cstr(buf, e->name) ||
            !gen_strbuf_append_cstr(buf, ",\"type\":") ||
            !gen_json_quote_cstr(buf, e->type != NULL ? e->type->name : NULL) ||
            !gen_strbuf_append_cstr(buf, ",\"value\":")) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = gen_value_to_json_impl(e->value, buf);
        if (rc != GEN_OK) {
            return rc;
        }
        if (!gen_strbuf_append_char(buf, '}')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }
    if (!gen_strbuf_append_cstr(buf, "],\"memberships\":[")) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    for (i = 0; i < document->memberships.count; i++) {
        GenMembershipRec *m = (GenMembershipRec *)document->memberships.items[i];
        if (i > 0 && !gen_strbuf_append_char(buf, ',')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (!gen_strbuf_append_cstr(buf, "{\"entity\":") || !gen_json_quote_cstr(buf, m->entity) ||
            !gen_strbuf_append_cstr(buf, ",\"set\":") || !gen_json_quote_cstr(buf, m->set) ||
            !gen_strbuf_append_char(buf, '}')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }
    if (!gen_strbuf_append_cstr(buf, "]}")) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    return GEN_OK;
}

GenResult gen_document_to_json(const GenDocument *document, char **out_text, size_t *out_length)
{
    GenStrBuf buf;
    GenResult rc;

    if (document == NULL || out_text == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_text = NULL;
    if (out_length != NULL) {
        *out_length = 0;
    }
    gen_strbuf_init(&buf);
    rc = gen_document_to_json_impl(document, &buf);
    if (rc != GEN_OK) {
        gen_strbuf_free(&buf);
        return rc;
    }
    *out_text = gen_strbuf_steal(&buf, out_length);
    return *out_text != NULL ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
}
