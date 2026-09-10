#include "query/query.h"

#include "error/error.h"
#include "parser/parser.h"

static GenQueryResult *gen_query_result_from_strvec(GenStrVec *vec)
{
    GenQueryResult *result = (GenQueryResult *)gen_calloc(1, sizeof(GenQueryResult));
    if (result == NULL) {
        return NULL;
    }
    result->names = vec->items;
    result->count = vec->count;
    vec->items = NULL;
    vec->count = 0;
    vec->capacity = 0;
    return result;
}

void gen_query_result_free(GenQueryResult *result)
{
    size_t i;
    if (result == NULL) {
        return;
    }
    for (i = 0; i < result->count; i++) {
        gen_free(result->names[i]);
    }
    gen_free(result->names);
    gen_free(result);
}

size_t gen_query_result_count(const GenQueryResult *result)
{
    return result != NULL ? result->count : 0;
}

const char *gen_query_result_name(const GenQueryResult *result, size_t index)
{
    if (result == NULL || index >= result->count) {
        return NULL;
    }
    return result->names[index];
}

static const GenValue *gen_walk_value_path(
    const GenValue *current,
    const GenAst *node,
    GenResult *err
)
{
    const GenValue *base;

    if (node == NULL) {
        *err = GEN_ERR_QUERY;
        return NULL;
    }
    if (node->kind == GEN_AST_PROP_ACCESS) {
        GenProp *prop;
        base = gen_walk_value_path(current, node->u.prop_access.base, err);
        if (base == NULL) {
            return NULL;
        }
        if (base->kind != GEN_VALUE_OBJECT) {
            *err = GEN_ERR_TYPE;
            return NULL;
        }
        HASH_FIND_STR(base->u.object.by_name, node->u.prop_access.property, prop);
        if (prop == NULL) {
            *err = GEN_ERR_NOT_FOUND;
            return NULL;
        }
        return prop->value;
    }
    if (node->kind == GEN_AST_INDEX_ACCESS) {
        size_t idx;
        base = gen_walk_value_path(current, node->u.index_access.base, err);
        if (base == NULL) {
            return NULL;
        }
        if (base->kind != GEN_VALUE_LIST) {
            *err = GEN_ERR_TYPE;
            return NULL;
        }
        if (node->u.index_access.index < 0) {
            *err = GEN_ERR_INDEX;
            return NULL;
        }
        idx = (size_t)node->u.index_access.index;
        if (idx >= base->u.list.count) {
            *err = GEN_ERR_INDEX;
            return NULL;
        }
        return base->u.list.items[idx];
    }
    if (node->kind == GEN_AST_IDENT) {
        return current;
    }
    *err = GEN_ERR_QUERY;
    return NULL;
}

static const char *gen_path_root_name(const GenAst *node)
{
    while (node != NULL) {
        if (node->kind == GEN_AST_IDENT) {
            return node->u.ident.name;
        }
        if (node->kind == GEN_AST_PROP_ACCESS) {
            node = node->u.prop_access.base;
            continue;
        }
        if (node->kind == GEN_AST_INDEX_ACCESS) {
            node = node->u.index_access.base;
            continue;
        }
        return NULL;
    }
    return NULL;
}

static GenResult gen_eval_path_on_doc(
    const GenDocument *document,
    const GenAst *path,
    GenValue **out_value
)
{
    const char *root;
    GenEntity *ent;
    const GenValue *leaf;
    GenResult err = GEN_OK;

    root = gen_path_root_name(path);
    if (root == NULL) {
        return GEN_ERR_QUERY;
    }
    ent = gen_entity_lookup(document, root);
    if (ent == NULL || ent->value == NULL) {
        return GEN_ERR_NOT_FOUND;
    }
    leaf = gen_walk_value_path(ent->value, path, &err);
    if (leaf == NULL) {
        return err != GEN_OK ? err : GEN_ERR_NOT_FOUND;
    }
    return gen_value_clone_impl(leaf, out_value);
}

GenResult gen_eval_path(const GenDocument *document, const char *path, GenValue **out_value)
{
    GenContext *ctx;
    GenAstProgram program;
    GenResult rc;
    char *wrapped;
    size_t n;

    if (document == NULL || path == NULL || out_value == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_value = NULL;
    n = strlen(path);
    wrapped = gen_strndup(path, n);
    if (wrapped == NULL) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    ctx = gen_context_create();
    if (ctx == NULL) {
        gen_free(wrapped);
        return GEN_ERR_OUT_OF_MEMORY;
    }
    rc = gen_parse(ctx, wrapped, n, GEN_PARSE_REPL, NULL, &program);
    if (rc != GEN_OK) {
        gen_context_free(ctx);
        gen_free(wrapped);
        return rc == GEN_ERR_PARSE || rc == GEN_ERR_LEX ? GEN_ERR_QUERY : rc;
    }
    if (program.root == NULL || program.root->u.program.count != 1u) {
        gen_ast_program_free(&program);
        gen_context_free(ctx);
        gen_free(wrapped);
        return GEN_ERR_QUERY;
    }
    rc = gen_eval_path_on_doc(document, program.root->u.program.items[0], out_value);
    gen_ast_program_free(&program);
    gen_context_free(ctx);
    gen_free(wrapped);
    return rc;
}

GenResult gen_get(const GenDocument *document, const char *path, GenValue **out_value)
{
    return gen_eval_path(document, path, out_value);
}

static GenResult gen_result_from_names(GenStrVec *names, GenQueryResult **out_result)
{
    *out_result = gen_query_result_from_strvec(names);
    if (*out_result == NULL) {
        gen_strvec_free_all(names);
        return GEN_ERR_OUT_OF_MEMORY;
    }
    return GEN_OK;
}

GenResult gen_types_of(
    const GenDocument *document,
    const char *name,
    GenQueryResult **out_result
)
{
    GenEntity *ent;
    GenTypeNode *type;
    GenStrVec names;
    GenResult rc;

    if (document == NULL || name == NULL || out_result == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_result = NULL;
    gen_strvec_init(&names);
    ent = gen_entity_lookup(document, name);
    if (ent != NULL) {
        type = ent->type;
        if (type == NULL) {
            return gen_result_from_names(&names, out_result);
        }
        if (!gen_strvec_push_copy(&names, type->name)) {
            gen_strvec_free_all(&names);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = gen_type_collect_ancestors(type, &names);
        if (rc != GEN_OK) {
            gen_strvec_free_all(&names);
            return rc;
        }
        return gen_result_from_names(&names, out_result);
    }
    type = gen_type_lookup(document, name);
    if (type == NULL) {
        gen_strvec_free(&names);
        return GEN_ERR_NOT_FOUND;
    }
    if (!gen_strvec_push_copy(&names, type->name)) {
        gen_strvec_free_all(&names);
        return GEN_ERR_OUT_OF_MEMORY;
    }
    rc = gen_type_collect_ancestors(type, &names);
    if (rc != GEN_OK) {
        gen_strvec_free_all(&names);
        return rc;
    }
    return gen_result_from_names(&names, out_result);
}

GenResult gen_ancestors_of(
    const GenDocument *document,
    const char *name,
    GenQueryResult **out_result
)
{
    GenTypeNode *type;
    GenStrVec names;
    GenResult rc;

    if (document == NULL || name == NULL || out_result == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_result = NULL;
    type = gen_type_lookup(document, name);
    if (type == NULL) {
        return GEN_ERR_NOT_FOUND;
    }
    gen_strvec_init(&names);
    rc = gen_type_collect_ancestors(type, &names);
    if (rc != GEN_OK) {
        gen_strvec_free_all(&names);
        return rc;
    }
    return gen_result_from_names(&names, out_result);
}

GenResult gen_descendants_of(
    const GenDocument *document,
    const char *name,
    GenQueryResult **out_result
)
{
    GenTypeNode *type;
    GenStrVec names;
    GenResult rc;

    if (document == NULL || name == NULL || out_result == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_result = NULL;
    type = gen_type_lookup(document, name);
    if (type == NULL) {
        return GEN_ERR_NOT_FOUND;
    }
    gen_strvec_init(&names);
    rc = gen_type_collect_descendants(type, &names);
    if (rc != GEN_OK) {
        gen_strvec_free_all(&names);
        return rc;
    }
    return gen_result_from_names(&names, out_result);
}

GenResult gen_memberships_of(
    const GenDocument *document,
    const char *name,
    GenQueryResult **out_result
)
{
    GenEntity *ent;
    GenStrVec names;
    size_t i;

    if (document == NULL || name == NULL || out_result == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_result = NULL;
    ent = gen_entity_lookup(document, name);
    if (ent == NULL) {
        return GEN_ERR_NOT_FOUND;
    }
    gen_strvec_init(&names);
    for (i = 0; i < ent->sets.count; i++) {
        GenSetNode *set = (GenSetNode *)ent->sets.items[i];
        if (!gen_strvec_push_copy(&names, set->name)) {
            gen_strvec_free_all(&names);
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }
    return gen_result_from_names(&names, out_result);
}

GenResult gen_set_members(
    const GenDocument *document,
    const char *set_name,
    GenQueryResult **out_result
)
{
    GenSetNode *set;
    GenStrVec names;
    size_t i;

    if (document == NULL || set_name == NULL || out_result == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_result = NULL;
    set = gen_set_lookup(document, set_name);
    if (set == NULL) {
        return GEN_ERR_NOT_FOUND;
    }
    gen_strvec_init(&names);
    for (i = 0; i < set->members.count; i++) {
        GenEntity *ent = (GenEntity *)set->members.items[i];
        if (!gen_strvec_push_copy(&names, ent->name)) {
            gen_strvec_free_all(&names);
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }
    return gen_result_from_names(&names, out_result);
}

GenResult gen_is_member(
    const GenDocument *document,
    const char *set_name,
    const char *entity_name,
    int *out_is_member
)
{
    GenSetNode *set;
    size_t i;

    if (document == NULL || set_name == NULL || entity_name == NULL || out_is_member == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_is_member = 0;
    set = gen_set_lookup(document, set_name);
    if (set == NULL) {
        return GEN_ERR_NOT_FOUND;
    }
    for (i = 0; i < set->members.count; i++) {
        GenEntity *ent = (GenEntity *)set->members.items[i];
        if (strcmp(ent->name, entity_name) == 0) {
            *out_is_member = 1;
            return GEN_OK;
        }
    }
    return GEN_OK;
}

static bool gen_type_is_ancestor(const GenTypeNode *from, const GenTypeNode *to)
{
    const GenTypeNode *cur = from;
    while (cur != NULL) {
        if (cur == to) {
            return true;
        }
        cur = cur->parent;
    }
    return false;
}

GenResult gen_type_path(
    const GenDocument *document,
    const char *from_name,
    const char *to_name,
    GenQueryResult **out_result
)
{
    GenTypeNode *from;
    GenTypeNode *to;
    GenStrVec names;
    const GenTypeNode *cur;

    if (document == NULL || from_name == NULL || to_name == NULL || out_result == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_result = NULL;
    from = gen_type_lookup(document, from_name);
    to = gen_type_lookup(document, to_name);
    if (from == NULL || to == NULL) {
        return GEN_ERR_NOT_FOUND;
    }
    gen_strvec_init(&names);
    if (gen_type_is_ancestor(from, to)) {
        cur = from;
        while (cur != NULL) {
            if (!gen_strvec_push_copy(&names, cur->name)) {
                gen_strvec_free_all(&names);
                return GEN_ERR_OUT_OF_MEMORY;
            }
            if (cur == to) {
                break;
            }
            cur = cur->parent;
        }
        return gen_result_from_names(&names, out_result);
    }
    if (gen_type_is_ancestor(to, from)) {
        cur = to;
        while (cur != NULL) {
            if (!gen_strvec_push_copy(&names, cur->name)) {
                gen_strvec_free_all(&names);
                return GEN_ERR_OUT_OF_MEMORY;
            }
            if (cur == from) {
                break;
            }
            cur = cur->parent;
        }
        return gen_result_from_names(&names, out_result);
    }
    gen_strvec_free(&names);
    return GEN_ERR_NOT_FOUND;
}

static bool gen_value_search(const GenValue *value, const char *query)
{
    size_t i;
    if (value == NULL || query == NULL) {
        return false;
    }
    switch (value->kind) {
    case GEN_VALUE_STRING:
        return value->u.string.data != NULL && strstr(value->u.string.data, query) != NULL;
    case GEN_VALUE_REFERENCE:
        return value->u.reference.data != NULL && strstr(value->u.reference.data, query) != NULL;
    case GEN_VALUE_LIST:
        for (i = 0; i < value->u.list.count; i++) {
            if (gen_value_search(value->u.list.items[i], query)) {
                return true;
            }
        }
        return false;
    case GEN_VALUE_OBJECT:
        for (i = 0; i < value->u.object.count; i++) {
            if (strstr(value->u.object.keys[i], query) != NULL) {
                return true;
            }
            if (gen_value_search(value->u.object.values[i], query)) {
                return true;
            }
        }
        return false;
    default:
        return false;
    }
}

GenResult gen_search(
    const GenDocument *document,
    const char *query,
    GenQueryResult **out_result
)
{
    GenStrVec names;
    size_t i;

    if (document == NULL || query == NULL || out_result == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_result = NULL;
    gen_strvec_init(&names);
    for (i = 0; i < document->types_order.count; i++) {
        GenTypeNode *t = (GenTypeNode *)document->types_order.items[i];
        if (strstr(t->name, query) != NULL && !gen_strvec_contains(&names, t->name)) {
            if (!gen_strvec_push_copy(&names, t->name)) {
                gen_strvec_free_all(&names);
                return GEN_ERR_OUT_OF_MEMORY;
            }
        }
    }
    for (i = 0; i < document->sets_order.count; i++) {
        GenSetNode *s = (GenSetNode *)document->sets_order.items[i];
        if (strstr(s->name, query) != NULL && !gen_strvec_contains(&names, s->name)) {
            if (!gen_strvec_push_copy(&names, s->name)) {
                gen_strvec_free_all(&names);
                return GEN_ERR_OUT_OF_MEMORY;
            }
        }
    }
    for (i = 0; i < document->entities_order.count; i++) {
        GenEntity *e = (GenEntity *)document->entities_order.items[i];
        if ((strstr(e->name, query) != NULL || gen_value_search(e->value, query)) &&
            !gen_strvec_contains(&names, e->name)) {
            if (!gen_strvec_push_copy(&names, e->name)) {
                gen_strvec_free_all(&names);
                return GEN_ERR_OUT_OF_MEMORY;
            }
        }
    }
    gen_strvec_sort(&names);
    return gen_result_from_names(&names, out_result);
}

GenResult gen_format_dyaz(const GenDocument *document, const char *name, char **out_text)
{
    GenQueryResult *types = NULL;
    GenQueryResult *sets = NULL;
    GenStrBuf buf;
    size_t i;
    GenResult rc;

    if (document == NULL || name == NULL || out_text == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_text = NULL;
    if (gen_entity_lookup(document, name) == NULL) {
        return GEN_ERR_NOT_FOUND;
    }
    rc = gen_types_of(document, name, &types);
    if (rc != GEN_OK) {
        return rc;
    }
    rc = gen_memberships_of(document, name, &sets);
    if (rc != GEN_OK) {
        gen_query_result_free(types);
        return rc;
    }
    gen_strbuf_init(&buf);
    if (!gen_strbuf_append_cstr(&buf, "Types:\n")) {
        gen_query_result_free(types);
        gen_query_result_free(sets);
        gen_strbuf_free(&buf);
        return GEN_ERR_OUT_OF_MEMORY;
    }
    if (types->count == 0) {
        if (!gen_strbuf_append_cstr(&buf, "  (none)\n")) {
            gen_query_result_free(types);
            gen_query_result_free(sets);
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }
    for (i = 0; i < types->count; i++) {
        if (!gen_strbuf_appendf(&buf, "  %s\n", types->names[i])) {
            gen_query_result_free(types);
            gen_query_result_free(sets);
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }
    if (!gen_strbuf_append_cstr(&buf, "\nSets:\n")) {
        gen_query_result_free(types);
        gen_query_result_free(sets);
        gen_strbuf_free(&buf);
        return GEN_ERR_OUT_OF_MEMORY;
    }
    if (sets->count == 0) {
        if (!gen_strbuf_append_cstr(&buf, "  (none)\n")) {
            gen_query_result_free(types);
            gen_query_result_free(sets);
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }
    for (i = 0; i < sets->count; i++) {
        if (!gen_strbuf_appendf(&buf, "  %s\n", sets->names[i])) {
            gen_query_result_free(types);
            gen_query_result_free(sets);
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }
    gen_query_result_free(types);
    gen_query_result_free(sets);
    *out_text = gen_strbuf_steal(&buf, NULL);
    return *out_text != NULL ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
}

GenResult gen_format_goster(const GenDocument *document, const char *name, char **out_text)
{
    GenEntity *ent;
    GenStrBuf buf;
    GenResult rc;

    if (document == NULL || name == NULL || out_text == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_text = NULL;
    ent = gen_entity_lookup(document, name);
    if (ent == NULL) {
        return GEN_ERR_NOT_FOUND;
    }
    gen_strbuf_init(&buf);
    if (ent->type != NULL) {
        if (!gen_strbuf_appendf(&buf, "%s : %s\n", ent->name, ent->type->name)) {
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
    } else {
        if (!gen_strbuf_appendf(&buf, "%s =\n", ent->name)) {
            gen_strbuf_free(&buf);
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }
    rc = gen_value_format_impl(ent->value, 0, &buf);
    if (rc != GEN_OK) {
        gen_strbuf_free(&buf);
        return rc;
    }
    if (!gen_strbuf_append_char(&buf, '\n')) {
        gen_strbuf_free(&buf);
        return GEN_ERR_OUT_OF_MEMORY;
    }
    *out_text = gen_strbuf_steal(&buf, NULL);
    return *out_text != NULL ? GEN_OK : GEN_ERR_OUT_OF_MEMORY;
}
