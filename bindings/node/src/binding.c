#define NAPI_VERSION 8
#define BUILDING_NODE_EXTENSION 1

#include <node_api.h>
#include <genlang.h>

#include <stdlib.h>
#include <string.h>

typedef struct {
    GenContext *ctx;
    GenDocument *doc;
} DocWrap;

static napi_value js_undefined(napi_env env)
{
    napi_value v;
    napi_get_undefined(env, &v);
    return v;
}

static napi_value js_null(napi_env env)
{
    napi_value v;
    napi_get_null(env, &v);
    return v;
}

static napi_value js_bool(napi_env env, int value)
{
    napi_value v;
    napi_get_boolean(env, value != 0, &v);
    return v;
}

static napi_value js_string(napi_env env, const char *s)
{
    napi_value v;
    if (s == NULL) {
        return js_null(env);
    }
    napi_create_string_utf8(env, s, NAPI_AUTO_LENGTH, &v);
    return v;
}

static napi_value js_int64(napi_env env, int64_t n)
{
    napi_value v;
    napi_create_int64(env, n, &v);
    return v;
}

static char *utf8_dup(napi_env env, napi_value value)
{
    size_t len = 0;
    char *s;
    napi_get_value_string_utf8(env, value, NULL, 0, &len);
    s = (char *)malloc(len + 1u);
    if (s == NULL) {
        return NULL;
    }
    napi_get_value_string_utf8(env, value, s, len + 1u, &len);
    return s;
}

static napi_value throw_error(napi_env env, const char *msg)
{
    napi_throw_error(env, NULL, msg);
    return NULL;
}

static napi_value throw_gen(napi_env env, GenContext *ctx, int rc, const char *fallback)
{
    napi_value err;
    napi_value msg;
    napi_value code;
    napi_value line;
    napi_value column;
    const GenError *e = ctx != NULL ? gen_context_last_error(ctx) : NULL;
    const char *text = fallback;
    size_t ln = 0;
    size_t col = 0;

    if (e != NULL && gen_error_message(e) != NULL && gen_error_message(e)[0] != '\0') {
        text = gen_error_message(e);
        ln = gen_error_line(e);
        col = gen_error_column(e);
    }
    napi_create_string_utf8(env, text, NAPI_AUTO_LENGTH, &msg);
    napi_create_error(env, NULL, msg, &err);
    napi_create_int32(env, rc, &code);
    napi_create_int64(env, (int64_t)ln, &line);
    napi_create_int64(env, (int64_t)col, &column);
    napi_set_named_property(env, err, "code", code);
    napi_set_named_property(env, err, "line", line);
    napi_set_named_property(env, err, "column", column);
    napi_throw(env, err);
    return NULL;
}

static napi_value value_to_js(napi_env env, const GenValue *value);

static napi_value value_to_js(napi_env env, const GenValue *value)
{
    napi_value out;
    size_t i;
    size_t n;

    if (value == NULL) {
        return js_null(env);
    }
    switch (gen_value_type(value)) {
    case GEN_VALUE_NULL:
        return js_null(env);
    case GEN_VALUE_BOOL:
        return js_bool(env, gen_value_bool(value));
    case GEN_VALUE_INT:
        return js_int64(env, gen_value_int(value));
    case GEN_VALUE_FLOAT:
        napi_create_double(env, gen_value_float(value), &out);
        return out;
    case GEN_VALUE_STRING:
        return js_string(env, gen_value_string(value));
    case GEN_VALUE_REFERENCE:
        napi_create_object(env, &out);
        napi_set_named_property(env, out, "$ref", js_string(env, gen_value_reference(value)));
        return out;
    case GEN_VALUE_LIST:
        n = gen_value_list_count(value);
        napi_create_array_with_length(env, n, &out);
        for (i = 0; i < n; i++) {
            const GenValue *item = NULL;
            gen_value_list_get(value, i, &item);
            napi_set_element(env, out, (uint32_t)i, value_to_js(env, item));
        }
        return out;
    case GEN_VALUE_OBJECT:
        n = gen_value_object_count(value);
        napi_create_object(env, &out);
        for (i = 0; i < n; i++) {
            const char *key = NULL;
            const GenValue *item = NULL;
            gen_value_object_key(value, i, &key);
            gen_value_object_get_index(value, i, &item);
            napi_set_named_property(env, out, key, value_to_js(env, item));
        }
        return out;
    default:
        return js_null(env);
    }
}

static void doc_finalize(napi_env env, void *data, void *hint)
{
    DocWrap *w = (DocWrap *)data;
    (void)env;
    (void)hint;
    if (w == NULL) {
        return;
    }
    gen_document_free(w->doc);
    gen_context_free(w->ctx);
    free(w);
}

static napi_value Document_close(napi_env env, napi_callback_info info);
static napi_value Document_types(napi_env env, napi_callback_info info);
static napi_value Document_sets(napi_env env, napi_callback_info info);
static napi_value Document_entities(napi_env env, napi_callback_info info);
static napi_value Document_typeParent(napi_env env, napi_callback_info info);
static napi_value Document_entityType(napi_env env, napi_callback_info info);
static napi_value Document_entity(napi_env env, napi_callback_info info);
static napi_value Document_get(napi_env env, napi_callback_info info);
static napi_value Document_ancestors(napi_env env, napi_callback_info info);
static napi_value Document_members(napi_env env, napi_callback_info info);
static napi_value Document_entitiesOf(napi_env env, napi_callback_info info);
static napi_value Document_isMember(napi_env env, napi_callback_info info);
static napi_value Document_serialize(napi_env env, napi_callback_info info);
static napi_value Document_toJson(napi_env env, napi_callback_info info);
static napi_value Document_toYaml(napi_env env, napi_callback_info info);
static napi_value Document_toBinary(napi_env env, napi_callback_info info);

static napi_value wrap_document(napi_env env, GenContext *ctx, GenDocument *doc)
{
    napi_value obj;
    DocWrap *w = (DocWrap *)calloc(1, sizeof(DocWrap));
    napi_property_descriptor props[] = {
        {"close", NULL, Document_close, NULL, NULL, NULL, napi_default, NULL},
        {"types", NULL, Document_types, NULL, NULL, NULL, napi_default, NULL},
        {"sets", NULL, Document_sets, NULL, NULL, NULL, napi_default, NULL},
        {"entities", NULL, Document_entities, NULL, NULL, NULL, napi_default, NULL},
        {"typeParent", NULL, Document_typeParent, NULL, NULL, NULL, napi_default, NULL},
        {"entityType", NULL, Document_entityType, NULL, NULL, NULL, napi_default, NULL},
        {"entity", NULL, Document_entity, NULL, NULL, NULL, napi_default, NULL},
        {"get", NULL, Document_get, NULL, NULL, NULL, napi_default, NULL},
        {"ancestors", NULL, Document_ancestors, NULL, NULL, NULL, napi_default, NULL},
        {"members", NULL, Document_members, NULL, NULL, NULL, napi_default, NULL},
        {"entitiesOf", NULL, Document_entitiesOf, NULL, NULL, NULL, napi_default, NULL},
        {"isMember", NULL, Document_isMember, NULL, NULL, NULL, napi_default, NULL},
        {"serialize", NULL, Document_serialize, NULL, NULL, NULL, napi_default, NULL},
        {"toJson", NULL, Document_toJson, NULL, NULL, NULL, napi_default, NULL},
        {"toYaml", NULL, Document_toYaml, NULL, NULL, NULL, napi_default, NULL},
        {"toBinary", NULL, Document_toBinary, NULL, NULL, NULL, napi_default, NULL},
    };

    if (w == NULL) {
        gen_document_free(doc);
        gen_context_free(ctx);
        return throw_error(env, "out of memory");
    }
    w->ctx = ctx;
    w->doc = doc;
    napi_create_object(env, &obj);
    napi_wrap(env, obj, w, doc_finalize, NULL, NULL);
    napi_define_properties(env, obj, sizeof(props) / sizeof(props[0]), props);
    return obj;
}

static DocWrap *unwrap_doc(napi_env env, napi_callback_info info, size_t want, napi_value *args)
{
    size_t argc = want;
    napi_value this;
    void *data = NULL;
    napi_get_cb_info(env, info, &argc, args, &this, NULL);
    if (want > 0 && argc < want) {
        throw_error(env, "missing argument");
        return NULL;
    }
    napi_unwrap(env, this, &data);
    if (data == NULL || ((DocWrap *)data)->doc == NULL) {
        throw_error(env, "document is closed");
        return NULL;
    }
    return (DocWrap *)data;
}

static napi_value Document_close(napi_env env, napi_callback_info info)
{
    napi_value this;
    void *data = NULL;
    DocWrap *w;
    napi_get_cb_info(env, info, NULL, NULL, &this, NULL);
    napi_unwrap(env, this, &data);
    w = (DocWrap *)data;
    if (w != NULL) {
        gen_document_free(w->doc);
        gen_context_free(w->ctx);
        w->doc = NULL;
        w->ctx = NULL;
    }
    return js_undefined(env);
}

static napi_value names_array(
    napi_env env,
    const GenDocument *doc,
    size_t count,
    GenResult (*getter)(const GenDocument *, size_t, const char **)
)
{
    napi_value arr;
    size_t i;
    napi_create_array_with_length(env, count, &arr);
    for (i = 0; i < count; i++) {
        const char *name = NULL;
        getter(doc, i, &name);
        napi_set_element(env, arr, (uint32_t)i, js_string(env, name));
    }
    return arr;
}

static napi_value Document_types(napi_env env, napi_callback_info info)
{
    DocWrap *w = unwrap_doc(env, info, 0, NULL);
    if (w == NULL) {
        return NULL;
    }
    return names_array(env, w->doc, gen_type_count(w->doc), gen_type_name);
}

static napi_value Document_sets(napi_env env, napi_callback_info info)
{
    DocWrap *w = unwrap_doc(env, info, 0, NULL);
    if (w == NULL) {
        return NULL;
    }
    return names_array(env, w->doc, gen_set_count(w->doc), gen_set_name);
}

static napi_value Document_entities(napi_env env, napi_callback_info info)
{
    DocWrap *w = unwrap_doc(env, info, 0, NULL);
    if (w == NULL) {
        return NULL;
    }
    return names_array(env, w->doc, gen_entity_count(w->doc), gen_entity_name);
}

static napi_value Document_typeParent(napi_env env, napi_callback_info info)
{
    napi_value args[1];
    DocWrap *w = unwrap_doc(env, info, 1, args);
    char *name;
    const char *parent = NULL;
    GenResult rc;
    if (w == NULL) {
        return NULL;
    }
    name = utf8_dup(env, args[0]);
    if (name == NULL) {
        return throw_error(env, "out of memory");
    }
    rc = gen_type_parent(w->doc, name, &parent);
    free(name);
    if (rc != GEN_OK) {
        return throw_gen(env, w->ctx, rc, "unknown type");
    }
    return js_string(env, parent);
}

static napi_value Document_entityType(napi_env env, napi_callback_info info)
{
    napi_value args[1];
    DocWrap *w = unwrap_doc(env, info, 1, args);
    char *name;
    const char *type = NULL;
    GenResult rc;
    if (w == NULL) {
        return NULL;
    }
    name = utf8_dup(env, args[0]);
    if (name == NULL) {
        return throw_error(env, "out of memory");
    }
    rc = gen_entity_type_name(w->doc, name, &type);
    free(name);
    if (rc != GEN_OK) {
        return throw_gen(env, w->ctx, rc, "unknown entity");
    }
    return js_string(env, type);
}

static napi_value Document_entity(napi_env env, napi_callback_info info)
{
    napi_value args[1];
    DocWrap *w = unwrap_doc(env, info, 1, args);
    char *name;
    const GenValue *value = NULL;
    GenResult rc;
    if (w == NULL) {
        return NULL;
    }
    name = utf8_dup(env, args[0]);
    if (name == NULL) {
        return throw_error(env, "out of memory");
    }
    rc = gen_entity_value(w->doc, name, &value);
    free(name);
    if (rc != GEN_OK) {
        return throw_gen(env, w->ctx, rc, "unknown entity");
    }
    return value_to_js(env, value);
}

static napi_value Document_get(napi_env env, napi_callback_info info)
{
    napi_value args[1];
    DocWrap *w = unwrap_doc(env, info, 1, args);
    char *path;
    GenValue *value = NULL;
    napi_value out;
    GenResult rc;
    if (w == NULL) {
        return NULL;
    }
    path = utf8_dup(env, args[0]);
    if (path == NULL) {
        return throw_error(env, "out of memory");
    }
    rc = gen_get(w->doc, path, &value);
    free(path);
    if (rc != GEN_OK) {
        return throw_gen(env, w->ctx, rc, "cannot evaluate path");
    }
    out = value_to_js(env, value);
    gen_value_free(value);
    return out;
}

static napi_value query_names(napi_env env, GenResult rc, GenQueryResult *result, DocWrap *w)
{
    napi_value arr;
    size_t i;
    size_t n;
    if (rc != GEN_OK) {
        return throw_gen(env, w->ctx, rc, "query failed");
    }
    n = gen_query_result_count(result);
    napi_create_array_with_length(env, n, &arr);
    for (i = 0; i < n; i++) {
        napi_set_element(env, arr, (uint32_t)i, js_string(env, gen_query_result_name(result, i)));
    }
    gen_query_result_free(result);
    return arr;
}

static napi_value Document_ancestors(napi_env env, napi_callback_info info)
{
    napi_value args[1];
    DocWrap *w = unwrap_doc(env, info, 1, args);
    char *name;
    GenQueryResult *result = NULL;
    GenResult rc;
    if (w == NULL) {
        return NULL;
    }
    name = utf8_dup(env, args[0]);
    if (name == NULL) {
        return throw_error(env, "out of memory");
    }
    rc = gen_ancestors_of(w->doc, name, &result);
    free(name);
    return query_names(env, rc, result, w);
}

static napi_value Document_members(napi_env env, napi_callback_info info)
{
    napi_value args[1];
    DocWrap *w = unwrap_doc(env, info, 1, args);
    char *name;
    GenQueryResult *result = NULL;
    GenResult rc;
    if (w == NULL) {
        return NULL;
    }
    name = utf8_dup(env, args[0]);
    if (name == NULL) {
        return throw_error(env, "out of memory");
    }
    rc = gen_set_members(w->doc, name, &result);
    free(name);
    return query_names(env, rc, result, w);
}

static napi_value Document_entitiesOf(napi_env env, napi_callback_info info)
{
    napi_value args[1];
    DocWrap *w = unwrap_doc(env, info, 1, args);
    char *name;
    GenQueryResult *result = NULL;
    GenResult rc;
    if (w == NULL) {
        return NULL;
    }
    name = utf8_dup(env, args[0]);
    if (name == NULL) {
        return throw_error(env, "out of memory");
    }
    rc = gen_entities_of(w->doc, name, &result);
    free(name);
    return query_names(env, rc, result, w);
}

static napi_value Document_isMember(napi_env env, napi_callback_info info)
{
    napi_value args[2];
    DocWrap *w = unwrap_doc(env, info, 2, args);
    char *set_name;
    char *entity;
    int flag = 0;
    GenResult rc;
    if (w == NULL) {
        return NULL;
    }
    set_name = utf8_dup(env, args[0]);
    entity = utf8_dup(env, args[1]);
    if (set_name == NULL || entity == NULL) {
        free(set_name);
        free(entity);
        return throw_error(env, "out of memory");
    }
    rc = gen_is_member(w->doc, set_name, entity, &flag);
    free(set_name);
    free(entity);
    if (rc != GEN_OK) {
        return throw_gen(env, w->ctx, rc, "unknown set");
    }
    return js_bool(env, flag);
}

static napi_value owned_string(napi_env env, GenResult rc, char *text, const char *fallback)
{
    napi_value out;
    if (rc != GEN_OK) {
        gen_string_free(text);
        return throw_error(env, fallback);
    }
    out = js_string(env, text);
    gen_string_free(text);
    return out;
}

static napi_value Document_serialize(napi_env env, napi_callback_info info)
{
    DocWrap *w = unwrap_doc(env, info, 0, NULL);
    char *text = NULL;
    GenResult rc;
    if (w == NULL) {
        return NULL;
    }
    rc = gen_document_serialize(w->doc, &text, NULL);
    return owned_string(env, rc, text, "serialize failed");
}

static napi_value Document_toJson(napi_env env, napi_callback_info info)
{
    DocWrap *w = unwrap_doc(env, info, 0, NULL);
    char *text = NULL;
    GenResult rc;
    if (w == NULL) {
        return NULL;
    }
    rc = gen_document_to_json(w->doc, &text, NULL);
    return owned_string(env, rc, text, "json conversion failed");
}

static napi_value Document_toYaml(napi_env env, napi_callback_info info)
{
    DocWrap *w = unwrap_doc(env, info, 0, NULL);
    char *text = NULL;
    GenResult rc;
    if (w == NULL) {
        return NULL;
    }
    rc = gen_document_to_yaml(w->doc, &text, NULL);
    return owned_string(env, rc, text, "yaml conversion failed");
}

static napi_value Document_toBinary(napi_env env, napi_callback_info info)
{
    DocWrap *w = unwrap_doc(env, info, 0, NULL);
    char *bytes = NULL;
    size_t length = 0;
    napi_value out;
    void *copy;
    GenResult rc;
    if (w == NULL) {
        return NULL;
    }
    rc = gen_document_to_binary(w->doc, &bytes, &length);
    if (rc != GEN_OK) {
        return throw_error(env, "binary conversion failed");
    }
    napi_create_buffer_copy(env, length, bytes, &copy, &out);
    gen_string_free(bytes);
    return out;
}

static napi_value parse_common(
    napi_env env,
    napi_callback_info info,
    int load_file
)
{
    size_t argc = 1;
    napi_value args[1];
    char *arg;
    GenContext *ctx;
    GenDocument *doc = NULL;
    GenResult rc;

    napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (argc < 1) {
        return throw_error(env, "missing argument");
    }
    arg = utf8_dup(env, args[0]);
    if (arg == NULL) {
        return throw_error(env, "out of memory");
    }
    ctx = gen_context_create();
    if (ctx == NULL) {
        free(arg);
        return throw_error(env, "out of memory");
    }
    if (load_file) {
        rc = gen_document_load_file(ctx, arg, &doc);
    } else {
        rc = gen_document_parse(ctx, arg, &doc);
    }
    free(arg);
    if (rc != GEN_OK) {
        napi_value err = throw_gen(env, ctx, rc, "parse failed");
        gen_document_free(doc);
        gen_context_free(ctx);
        return err;
    }
    return wrap_document(env, ctx, doc);
}

static napi_value Parse(napi_env env, napi_callback_info info)
{
    return parse_common(env, info, 0);
}

static napi_value Load(napi_env env, napi_callback_info info)
{
    return parse_common(env, info, 1);
}

static napi_value Version(napi_env env, napi_callback_info info)
{
    (void)info;
    return js_string(env, gen_version());
}

static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor props[] = {
        {"version", NULL, Version, NULL, NULL, NULL, napi_enumerable, NULL},
        {"parse", NULL, Parse, NULL, NULL, NULL, napi_enumerable, NULL},
        {"load", NULL, Load, NULL, NULL, NULL, napi_enumerable, NULL},
    };
    napi_define_properties(env, exports, 3, props);
    return exports;
}

NAPI_MODULE(genlang, Init)
