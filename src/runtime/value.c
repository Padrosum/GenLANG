#include "runtime/runtime.h"

#include "error/error.h"

GenValue *gen_value_new(GenValueType kind)
{
    GenValue *v = (GenValue *)gen_calloc(1, sizeof(GenValue));
    if (v == NULL) {
        return NULL;
    }
    v->kind = kind;
    return v;
}

void gen_value_destroy(GenValue *value)
{
    size_t i;

    if (value == NULL) {
        return;
    }
    switch (value->kind) {
    case GEN_VALUE_STRING:
        gen_string_clear(&value->u.string);
        break;
    case GEN_VALUE_REFERENCE:
        gen_string_clear(&value->u.reference);
        break;
    case GEN_VALUE_LIST:
        for (i = 0; i < value->u.list.count; i++) {
            gen_value_destroy(value->u.list.items[i]);
        }
        gen_free(value->u.list.items);
        break;
    case GEN_VALUE_OBJECT: {
        GenProp *prop;
        GenProp *tmp;
        HASH_ITER(hh, value->u.object.by_name, prop, tmp) {
            HASH_DEL(value->u.object.by_name, prop);
            gen_free(prop->key);
            gen_free(prop);
        }
        for (i = 0; i < value->u.object.count; i++) {
            gen_value_destroy(value->u.object.values[i]);
            gen_free(value->u.object.keys[i]);
        }
        gen_free(value->u.object.keys);
        gen_free(value->u.object.values);
        break;
    }
    default:
        break;
    }
    gen_free(value);
}

void gen_value_free(GenValue *value)
{
    gen_value_destroy(value);
}

GenResult gen_value_clone_impl(const GenValue *value, GenValue **out_value)
{
    GenValue *copy;
    size_t i;

    if (value == NULL || out_value == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    copy = gen_value_new(value->kind);
    if (copy == NULL) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    switch (value->kind) {
    case GEN_VALUE_NULL:
        break;
    case GEN_VALUE_BOOL:
        copy->u.boolean = value->u.boolean;
        break;
    case GEN_VALUE_INT:
        copy->u.integer = value->u.integer;
        break;
    case GEN_VALUE_FLOAT:
        copy->u.floating = value->u.floating;
        break;
    case GEN_VALUE_STRING:
        if (!gen_string_init_copy(&copy->u.string, value->u.string.data, value->u.string.length)) {
            gen_value_destroy(copy);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        break;
    case GEN_VALUE_REFERENCE:
        if (!gen_string_init_copy(
                &copy->u.reference, value->u.reference.data, value->u.reference.length
            )) {
            gen_value_destroy(copy);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        break;
    case GEN_VALUE_LIST:
        if (value->u.list.count > 0) {
            copy->u.list.items =
                (GenValue **)gen_calloc(value->u.list.count, sizeof(GenValue *));
            if (copy->u.list.items == NULL) {
                gen_value_destroy(copy);
                return GEN_ERR_OUT_OF_MEMORY;
            }
        }
        copy->u.list.count = value->u.list.count;
        for (i = 0; i < value->u.list.count; i++) {
            GenResult rc = gen_value_clone_impl(value->u.list.items[i], &copy->u.list.items[i]);
            if (rc != GEN_OK) {
                gen_value_destroy(copy);
                return rc;
            }
        }
        break;
    case GEN_VALUE_OBJECT:
        if (value->u.object.count > 0) {
            copy->u.object.keys = (char **)gen_calloc(value->u.object.count, sizeof(char *));
            copy->u.object.values =
                (GenValue **)gen_calloc(value->u.object.count, sizeof(GenValue *));
            if (copy->u.object.keys == NULL || copy->u.object.values == NULL) {
                gen_value_destroy(copy);
                return GEN_ERR_OUT_OF_MEMORY;
            }
        }
        copy->u.object.count = value->u.object.count;
        for (i = 0; i < value->u.object.count; i++) {
            GenProp *prop;
            GenResult rc;
            copy->u.object.keys[i] = gen_strdup(value->u.object.keys[i]);
            if (copy->u.object.keys[i] == NULL) {
                gen_value_destroy(copy);
                return GEN_ERR_OUT_OF_MEMORY;
            }
            rc = gen_value_clone_impl(value->u.object.values[i], &copy->u.object.values[i]);
            if (rc != GEN_OK) {
                gen_value_destroy(copy);
                return rc;
            }
            prop = (GenProp *)gen_calloc(1, sizeof(GenProp));
            if (prop == NULL) {
                gen_value_destroy(copy);
                return GEN_ERR_OUT_OF_MEMORY;
            }
            prop->key = gen_strdup(copy->u.object.keys[i]);
            prop->value = copy->u.object.values[i];
            if (prop->key == NULL) {
                gen_free(prop);
                gen_value_destroy(copy);
                return GEN_ERR_OUT_OF_MEMORY;
            }
            HASH_ADD_KEYPTR(hh, copy->u.object.by_name, prop->key, strlen(prop->key), prop);
        }
        break;
    default:
        gen_value_destroy(copy);
        return GEN_ERR_TYPE;
    }
    *out_value = copy;
    return GEN_OK;
}

GenResult gen_value_clone(const GenValue *value, GenValue **out_value)
{
    return gen_value_clone_impl(value, out_value);
}

static bool gen_append_indent(GenStrBuf *buf, int indent)
{
    int i;
    for (i = 0; i < indent; i++) {
        if (!gen_strbuf_append_cstr(buf, "    ")) {
            return false;
        }
    }
    return true;
}

static bool gen_escape_append(GenStrBuf *buf, const char *data, size_t length)
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
        } else {
            if (!gen_strbuf_append_char(buf, c)) {
                return false;
            }
        }
    }
    return gen_strbuf_append_char(buf, '"');
}

GenResult gen_value_format_impl(const GenValue *value, int indent, GenStrBuf *buf)
{
    size_t i;

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
        return gen_escape_append(buf, value->u.string.data, value->u.string.length)
                   ? GEN_OK
                   : GEN_ERR_OUT_OF_MEMORY;
    case GEN_VALUE_REFERENCE:
        return gen_strbuf_appendf(buf, "@%s", value->u.reference.data ? value->u.reference.data : "")
                   ? GEN_OK
                   : GEN_ERR_OUT_OF_MEMORY;
    case GEN_VALUE_LIST:
        if (!gen_strbuf_append_char(buf, '[')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        for (i = 0; i < value->u.list.count; i++) {
            GenResult rc;
            if (i > 0 && !gen_strbuf_append_cstr(buf, ", ")) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
            rc = gen_value_format_impl(value->u.list.items[i], indent, buf);
            if (rc != GEN_OK) {
                return rc;
            }
        }
        return gen_strbuf_append_char(buf, ']') ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
    case GEN_VALUE_OBJECT:
        if (value->u.object.count == 0) {
            return gen_strbuf_append_cstr(buf, "{}") ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
        }
        if (!gen_strbuf_append_cstr(buf, "{\n")) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        for (i = 0; i < value->u.object.count; i++) {
            GenResult rc;
            if (!gen_append_indent(buf, indent + 1)) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
            if (!gen_strbuf_appendf(buf, "%s = ", value->u.object.keys[i])) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
            rc = gen_value_format_impl(value->u.object.values[i], indent + 1, buf);
            if (rc != GEN_OK) {
                return rc;
            }
            if (!gen_strbuf_append_char(buf, '\n')) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
        }
        if (!gen_append_indent(buf, indent) || !gen_strbuf_append_char(buf, '}')) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        return GEN_OK;
    default:
        return GEN_ERR_TYPE;
    }
}

GenValueType gen_value_type(const GenValue *value)
{
    return value != NULL ? value->kind : GEN_VALUE_NULL;
}

int gen_value_bool(const GenValue *value)
{
    return value != NULL && value->kind == GEN_VALUE_BOOL && value->u.boolean;
}

int64_t gen_value_int(const GenValue *value)
{
    return value != NULL && value->kind == GEN_VALUE_INT ? value->u.integer : 0;
}

double gen_value_float(const GenValue *value)
{
    return value != NULL && value->kind == GEN_VALUE_FLOAT ? value->u.floating : 0.0;
}

const char *gen_value_string(const GenValue *value)
{
    if (value == NULL || value->kind != GEN_VALUE_STRING) {
        return NULL;
    }
    return value->u.string.data;
}

size_t gen_value_string_length(const GenValue *value)
{
    if (value == NULL || value->kind != GEN_VALUE_STRING) {
        return 0;
    }
    return value->u.string.length;
}

const char *gen_value_reference(const GenValue *value)
{
    if (value == NULL || value->kind != GEN_VALUE_REFERENCE) {
        return NULL;
    }
    return value->u.reference.data;
}

size_t gen_value_list_count(const GenValue *value)
{
    if (value == NULL || value->kind != GEN_VALUE_LIST) {
        return 0;
    }
    return value->u.list.count;
}

GenResult gen_value_list_get(const GenValue *value, size_t index, const GenValue **out_item)
{
    if (value == NULL || out_item == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    if (value->kind != GEN_VALUE_LIST) {
        return GEN_ERR_TYPE;
    }
    if (index >= value->u.list.count) {
        return GEN_ERR_INDEX;
    }
    *out_item = value->u.list.items[index];
    return GEN_OK;
}

size_t gen_value_object_count(const GenValue *value)
{
    if (value == NULL || value->kind != GEN_VALUE_OBJECT) {
        return 0;
    }
    return value->u.object.count;
}

GenResult gen_value_object_key(const GenValue *value, size_t index, const char **out_key)
{
    if (value == NULL || out_key == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    if (value->kind != GEN_VALUE_OBJECT) {
        return GEN_ERR_TYPE;
    }
    if (index >= value->u.object.count) {
        return GEN_ERR_INDEX;
    }
    *out_key = value->u.object.keys[index];
    return GEN_OK;
}

GenResult gen_value_object_get(const GenValue *value, const char *key, const GenValue **out_item)
{
    GenProp *prop;

    if (value == NULL || key == NULL || out_item == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    if (value->kind != GEN_VALUE_OBJECT) {
        return GEN_ERR_TYPE;
    }
    HASH_FIND_STR(value->u.object.by_name, key, prop);
    if (prop == NULL) {
        return GEN_ERR_NOT_FOUND;
    }
    *out_item = prop->value;
    return GEN_OK;
}

GenResult gen_value_object_get_index(
    const GenValue *value,
    size_t index,
    const GenValue **out_item
)
{
    if (value == NULL || out_item == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    if (value->kind != GEN_VALUE_OBJECT) {
        return GEN_ERR_TYPE;
    }
    if (index >= value->u.object.count) {
        return GEN_ERR_INDEX;
    }
    *out_item = value->u.object.values[index];
    return GEN_OK;
}

GenResult gen_value_format(const GenValue *value, char **out_text, size_t *out_length)
{
    GenStrBuf buf;
    GenResult rc;

    if (value == NULL || out_text == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    gen_strbuf_init(&buf);
    rc = gen_value_format_impl(value, 0, &buf);
    if (rc != GEN_OK) {
        gen_strbuf_free(&buf);
        return rc;
    }
    *out_text = gen_strbuf_steal(&buf, out_length);
    if (*out_text == NULL) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    return GEN_OK;
}

GenResult gen_value_object_put(GenValue *object, const char *key, GenValue *item)
{
    GenProp *prop;
    char **keys;
    GenValue **values;
    size_t n;

    if (object == NULL || key == NULL || item == NULL || object->kind != GEN_VALUE_OBJECT) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    HASH_FIND_STR(object->u.object.by_name, key, prop);
    if (prop != NULL) {
        return GEN_ERR_DUPLICATE;
    }
    n = object->u.object.count + 1u;
    keys = (char **)gen_realloc(object->u.object.keys, n * sizeof(char *));
    values = (GenValue **)gen_realloc(object->u.object.values, n * sizeof(GenValue *));
    if (keys == NULL || values == NULL) {
        if (keys != object->u.object.keys) {
            gen_free(keys);
        }
        if (values != object->u.object.values) {
            gen_free(values);
        }
        return GEN_ERR_OUT_OF_MEMORY;
    }
    object->u.object.keys = keys;
    object->u.object.values = values;
    object->u.object.keys[object->u.object.count] = gen_strdup(key);
    object->u.object.values[object->u.object.count] = item;
    if (object->u.object.keys[object->u.object.count] == NULL) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    prop = (GenProp *)gen_calloc(1, sizeof(GenProp));
    if (prop == NULL) {
        gen_free(object->u.object.keys[object->u.object.count]);
        return GEN_ERR_OUT_OF_MEMORY;
    }
    prop->key = gen_strdup(key);
    prop->value = item;
    if (prop->key == NULL) {
        gen_free(prop);
        gen_free(object->u.object.keys[object->u.object.count]);
        return GEN_ERR_OUT_OF_MEMORY;
    }
    HASH_ADD_KEYPTR(hh, object->u.object.by_name, prop->key, strlen(prop->key), prop);
    object->u.object.count = n;
    return GEN_OK;
}
