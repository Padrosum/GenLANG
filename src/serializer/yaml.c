#include "serializer/yaml.h"

static bool yaml_append_quoted(GenStrBuf *buf, const char *data, size_t length)
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

static bool yaml_is_reserved_plain(const char *s)
{
    return strcmp(s, "null") == 0 || strcmp(s, "Null") == 0 || strcmp(s, "NULL") == 0 ||
           strcmp(s, "true") == 0 || strcmp(s, "True") == 0 || strcmp(s, "TRUE") == 0 ||
           strcmp(s, "false") == 0 || strcmp(s, "False") == 0 || strcmp(s, "FALSE") == 0 ||
           strcmp(s, "~") == 0;
}

static bool yaml_plain_ok(const char *s)
{
    size_t n;
    size_t i = 0;
    uint32_t cp;
    size_t sz;

    if (s == NULL || s[0] == '\0' || yaml_is_reserved_plain(s)) {
        return false;
    }
    if (s[0] == '$') {
        n = strlen(s);
        if (n < 2u) {
            return false;
        }
        i = 1;
        while (i < n) {
            unsigned char c = (unsigned char)s[i];
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                  c == '_')) {
                return false;
            }
            i++;
        }
        return true;
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

static bool yaml_emit_string(GenStrBuf *buf, const char *s)
{
    if (s == NULL) {
        return gen_strbuf_append_cstr(buf, "null");
    }
    if (yaml_plain_ok(s)) {
        return gen_strbuf_append_cstr(buf, s);
    }
    return yaml_append_quoted(buf, s, strlen(s));
}

static bool yaml_indent(GenStrBuf *buf, int depth)
{
    int i;
    for (i = 0; i < depth; i++) {
        if (!gen_strbuf_append_cstr(buf, "  ")) {
            return false;
        }
    }
    return true;
}

static bool yaml_is_complex(const GenValue *value)
{
    if (value == NULL) {
        return false;
    }
    if (value->kind == GEN_VALUE_LIST) {
        return value->u.list.count > 0u;
    }
    if (value->kind == GEN_VALUE_OBJECT) {
        return value->u.object.count > 0u;
    }
    return false;
}

static GenResult yaml_emit_scalar(const GenValue *value, GenStrBuf *buf)
{
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
        return yaml_append_quoted(buf, value->u.string.data, value->u.string.length)
                   ? GEN_OK
                   : GEN_ERR_OUT_OF_MEMORY;
    case GEN_VALUE_REFERENCE:
        if (!gen_strbuf_append_cstr(buf, "$ref: ") ||
            !yaml_emit_string(buf, value->u.reference.data)) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        return GEN_OK;
    case GEN_VALUE_LIST:
        return gen_strbuf_append_cstr(buf, "[]") ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    case GEN_VALUE_OBJECT:
        return gen_strbuf_append_cstr(buf, "{}") ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    default:
        return GEN_ERR_TYPE;
    }
}

static GenResult yaml_emit_value(const GenValue *value, GenStrBuf *buf, int depth, bool continue_line);

static GenResult yaml_emit_list(const GenValue *value, GenStrBuf *buf, int depth)
{
    size_t i;
    GenResult rc;

    for (i = 0; i < value->u.list.count; i++) {
        if (!yaml_indent(buf, depth) || !gen_strbuf_append_cstr(buf, "- ")) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = yaml_emit_value(value->u.list.items[i], buf, depth + 1, true);
        if (rc != GEN_OK) {
            return rc;
        }
    }
    return GEN_OK;
}

static GenResult yaml_emit_object(const GenValue *value, GenStrBuf *buf, int depth, bool first_continues)
{
    size_t i;
    GenResult rc;

    for (i = 0; i < value->u.object.count; i++) {
        if (i == 0 && first_continues) {
            /* first key stays on the "- " line */
        } else if (!yaml_indent(buf, depth)) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (!yaml_emit_string(buf, value->u.object.keys[i]) ||
            !gen_strbuf_append_cstr(buf, ":")) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (yaml_is_complex(value->u.object.values[i])) {
            if (!gen_strbuf_append_char(buf, '\n')) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
            rc = yaml_emit_value(value->u.object.values[i], buf, depth + 1, false);
        } else {
            if (!gen_strbuf_append_char(buf, ' ')) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
            rc = yaml_emit_value(value->u.object.values[i], buf, depth + 1, true);
        }
        if (rc != GEN_OK) {
            return rc;
        }
    }
    return GEN_OK;
}

static GenResult yaml_emit_value(const GenValue *value, GenStrBuf *buf, int depth, bool continue_line)
{
    if (value == NULL || buf == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    if (value->kind == GEN_VALUE_REFERENCE) {
        if (!continue_line && !yaml_indent(buf, depth)) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (yaml_emit_scalar(value, buf) != GEN_OK || !gen_strbuf_append_char(buf, '\n')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        return GEN_OK;
    }
    if (value->kind == GEN_VALUE_LIST) {
        if (value->u.list.count == 0u) {
            if (!continue_line && !yaml_indent(buf, depth)) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
            return gen_strbuf_append_cstr(buf, "[]\n") ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
        }
        if (continue_line && !gen_strbuf_append_char(buf, '\n')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        return yaml_emit_list(value, buf, depth);
    }
    if (value->kind == GEN_VALUE_OBJECT) {
        if (value->u.object.count == 0u) {
            if (!continue_line && !yaml_indent(buf, depth)) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
            return gen_strbuf_append_cstr(buf, "{}\n") ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
        }
        if (continue_line) {
            return yaml_emit_object(value, buf, depth, true);
        }
        return yaml_emit_object(value, buf, depth, false);
    }
    if (!continue_line && !yaml_indent(buf, depth)) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    if (yaml_emit_scalar(value, buf) != GEN_OK || !gen_strbuf_append_char(buf, '\n')) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    return GEN_OK;
}

GenResult gen_value_to_yaml_impl(const GenValue *value, GenStrBuf *buf, int depth)
{
    return yaml_emit_value(value, buf, depth, false);
}

static GenResult yaml_emit_props_or_null(const GenValue *props, GenStrBuf *buf, int depth)
{
    if (props == NULL) {
        return gen_strbuf_append_cstr(buf, " null\n") ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    }
    if (!yaml_is_complex(props)) {
        if (!gen_strbuf_append_char(buf, ' ')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        return yaml_emit_value(props, buf, depth, true);
    }
    if (!gen_strbuf_append_char(buf, '\n')) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    return yaml_emit_value(props, buf, depth, false);
}

GenResult gen_value_to_yaml(const GenValue *value, char **out_text, size_t *out_length)
{
    GenStrBuf buf;
    GenResult rc;

    if (value == NULL || out_text == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_text = NULL;
    gen_strbuf_init(&buf);
    rc = gen_value_to_yaml_impl(value, &buf, 0);
    if (rc != GEN_OK) {
        gen_strbuf_free(&buf);
        return rc;
    }
    *out_text = gen_strbuf_steal(&buf, out_length);
    return *out_text != NULL ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
}

GenResult gen_document_to_yaml(const GenDocument *document, char **out_text, size_t *out_length)
{
    GenStrBuf buf;
    size_t i;
    GenResult rc;

    if (document == NULL || out_text == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_text = NULL;
    if (out_length != NULL) {
        *out_length = 0;
    }
    gen_strbuf_init(&buf);

    if (document->types_order.count == 0u) {
        if (!gen_strbuf_append_cstr(&buf, "types: []\n")) {
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
    } else if (!gen_strbuf_append_cstr(&buf, "types:\n")) {
        gen_strbuf_free(&buf);
        return GEN_ERR_OUT_OF_MEMORY;
    }
    for (i = 0; i < document->types_order.count; i++) {
        GenTypeNode *t = (GenTypeNode *)document->types_order.items[i];
        const char *kind = t->kind == GEN_TYPE_KIND_GENUS ? "cins" : "tur";
        if (!gen_strbuf_append_cstr(&buf, "  - kind: ") || !yaml_emit_string(&buf, kind) ||
            !gen_strbuf_append_cstr(&buf, "\n    name: ") || !yaml_emit_string(&buf, t->name) ||
            !gen_strbuf_append_cstr(&buf, "\n    parent: ") ||
            !yaml_emit_string(&buf, t->parent != NULL ? t->parent->name : NULL) ||
            !gen_strbuf_append_cstr(&buf, "\n    properties:")) {
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = yaml_emit_props_or_null(t->props, &buf, 3);
        if (rc != GEN_OK) {
            gen_strbuf_free(&buf);
            return rc;
        }
    }

    if (document->sets_order.count == 0u) {
        if (!gen_strbuf_append_cstr(&buf, "sets: []\n")) {
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
    } else if (!gen_strbuf_append_cstr(&buf, "sets:\n")) {
        gen_strbuf_free(&buf);
        return GEN_ERR_OUT_OF_MEMORY;
    }
    for (i = 0; i < document->sets_order.count; i++) {
        GenSetNode *s = (GenSetNode *)document->sets_order.items[i];
        if (!gen_strbuf_append_cstr(&buf, "  - name: ") || !yaml_emit_string(&buf, s->name) ||
            !gen_strbuf_append_cstr(&buf, "\n    properties:")) {
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = yaml_emit_props_or_null(s->props, &buf, 3);
        if (rc != GEN_OK) {
            gen_strbuf_free(&buf);
            return rc;
        }
    }

    if (document->entities_order.count == 0u) {
        if (!gen_strbuf_append_cstr(&buf, "entities: []\n")) {
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
    } else if (!gen_strbuf_append_cstr(&buf, "entities:\n")) {
        gen_strbuf_free(&buf);
        return GEN_ERR_OUT_OF_MEMORY;
    }
    for (i = 0; i < document->entities_order.count; i++) {
        GenEntity *e = (GenEntity *)document->entities_order.items[i];
        if (!gen_strbuf_append_cstr(&buf, "  - name: ") || !yaml_emit_string(&buf, e->name) ||
            !gen_strbuf_append_cstr(&buf, "\n    type: ") ||
            !yaml_emit_string(&buf, e->type != NULL ? e->type->name : NULL) ||
            !gen_strbuf_append_cstr(&buf, "\n    value:")) {
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (yaml_is_complex(e->value)) {
            if (!gen_strbuf_append_char(&buf, '\n')) {
                gen_strbuf_free(&buf);
                return GEN_ERR_OUT_OF_MEMORY;
            }
            rc = yaml_emit_value(e->value, &buf, 3, false);
        } else {
            if (!gen_strbuf_append_char(&buf, ' ')) {
                gen_strbuf_free(&buf);
                return GEN_ERR_OUT_OF_MEMORY;
            }
            rc = yaml_emit_value(e->value, &buf, 3, true);
        }
        if (rc != GEN_OK) {
            gen_strbuf_free(&buf);
            return rc;
        }
    }

    if (document->memberships.count == 0u) {
        if (!gen_strbuf_append_cstr(&buf, "memberships: []\n")) {
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
    } else if (!gen_strbuf_append_cstr(&buf, "memberships:\n")) {
        gen_strbuf_free(&buf);
        return GEN_ERR_OUT_OF_MEMORY;
    }
    for (i = 0; i < document->memberships.count; i++) {
        GenMembershipRec *m = (GenMembershipRec *)document->memberships.items[i];
        if (!gen_strbuf_append_cstr(&buf, "  - entity: ") || !yaml_emit_string(&buf, m->entity) ||
            !gen_strbuf_append_cstr(&buf, "\n    set: ") || !yaml_emit_string(&buf, m->set) ||
            !gen_strbuf_append_char(&buf, '\n')) {
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }

    *out_text = gen_strbuf_steal(&buf, out_length);
    return *out_text != NULL ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
}
