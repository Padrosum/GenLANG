#include "semantic/semantic.h"

#include "error/error.h"

typedef enum {
    GEN_COLOR_WHITE = 0,
    GEN_COLOR_GRAY,
    GEN_COLOR_BLACK
} GenColor;

typedef struct {
    char *name;
    int color;
    UT_hash_handle hh;
} GenColorEntry;

static void gen_colors_free(GenColorEntry *map)
{
    GenColorEntry *e;
    GenColorEntry *tmp;
    HASH_ITER(hh, map, e, tmp) {
        HASH_DEL(map, e);
        gen_free(e->name);
        gen_free(e);
    }
}

static GenResult gen_ast_to_value(
    GenContext *ctx,
    const GenAst *node,
    size_t depth,
    GenValue **out_value
)
{
    GenValue *v;
    size_t i;
    GenResult rc;

    if (node == NULL || out_value == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    if (depth > ctx->limits.max_nesting_depth) {
        gen_context_set_error(
            ctx, GEN_ERR_SEMANTIC, node->line, node->column, node->offset, "maximum nesting depth exceeded"
        );
        return GEN_ERR_SEMANTIC;
    }
    switch (node->kind) {
    case GEN_AST_NULL:
        v = gen_value_new(GEN_VALUE_NULL);
        break;
    case GEN_AST_BOOL:
        v = gen_value_new(GEN_VALUE_BOOL);
        if (v != NULL) {
            v->u.boolean = node->u.boolean;
        }
        break;
    case GEN_AST_INT:
        v = gen_value_new(GEN_VALUE_INT);
        if (v != NULL) {
            v->u.integer = node->u.integer;
        }
        break;
    case GEN_AST_FLOAT:
        v = gen_value_new(GEN_VALUE_FLOAT);
        if (v != NULL) {
            v->u.floating = node->u.floating;
        }
        break;
    case GEN_AST_STRING:
        v = gen_value_new(GEN_VALUE_STRING);
        if (v == NULL) {
            break;
        }
        if (!gen_string_init_copy(&v->u.string, node->u.string.data, node->u.string.length)) {
            gen_value_destroy(v);
            gen_context_set_error(
                ctx, GEN_ERR_OUT_OF_MEMORY, node->line, node->column, node->offset, "out of memory"
            );
            return GEN_ERR_OUT_OF_MEMORY;
        }
        break;
    case GEN_AST_REFERENCE:
        v = gen_value_new(GEN_VALUE_REFERENCE);
        if (v == NULL) {
            break;
        }
        if (!gen_string_init_copy(
                &v->u.reference, node->u.reference.name, strlen(node->u.reference.name)
            )) {
            gen_value_destroy(v);
            gen_context_set_error(
                ctx, GEN_ERR_OUT_OF_MEMORY, node->line, node->column, node->offset, "out of memory"
            );
            return GEN_ERR_OUT_OF_MEMORY;
        }
        break;
    case GEN_AST_LIST:
        if (node->u.list.count > ctx->limits.max_list_length) {
            gen_context_set_error(
                ctx, GEN_ERR_SEMANTIC, node->line, node->column, node->offset, "list exceeds maximum length"
            );
            return GEN_ERR_SEMANTIC;
        }
        v = gen_value_new(GEN_VALUE_LIST);
        if (v == NULL) {
            break;
        }
        if (node->u.list.count > 0) {
            v->u.list.items = (GenValue **)gen_calloc(node->u.list.count, sizeof(GenValue *));
            if (v->u.list.items == NULL) {
                gen_value_destroy(v);
                gen_context_set_error(
                    ctx, GEN_ERR_OUT_OF_MEMORY, node->line, node->column, node->offset, "out of memory"
                );
                return GEN_ERR_OUT_OF_MEMORY;
            }
        }
        v->u.list.count = node->u.list.count;
        for (i = 0; i < node->u.list.count; i++) {
            rc = gen_ast_to_value(ctx, node->u.list.items[i], depth + 1u, &v->u.list.items[i]);
            if (rc != GEN_OK) {
                gen_value_destroy(v);
                return rc;
            }
        }
        break;
    case GEN_AST_OBJECT:
        if (node->u.object.count > ctx->limits.max_object_properties) {
            gen_context_set_error(
                ctx,
                GEN_ERR_SEMANTIC,
                node->line,
                node->column,
                node->offset,
                "object exceeds maximum property count"
            );
            return GEN_ERR_SEMANTIC;
        }
        v = gen_value_new(GEN_VALUE_OBJECT);
        if (v == NULL) {
            break;
        }
        for (i = 0; i < node->u.object.count; i++) {
            GenValue *child = NULL;
            rc = gen_ast_to_value(ctx, node->u.object.values[i], depth + 1u, &child);
            if (rc != GEN_OK) {
                gen_value_destroy(v);
                return rc;
            }
            rc = gen_value_object_put(v, node->u.object.keys[i], child);
            if (rc != GEN_OK) {
                gen_value_destroy(child);
                gen_value_destroy(v);
                if (rc == GEN_ERR_DUPLICATE) {
                    gen_context_set_error(
                        ctx,
                        GEN_ERR_DUPLICATE,
                        node->line,
                        node->column,
                        node->offset,
                        "duplicate property '%s'",
                        node->u.object.keys[i]
                    );
                    return GEN_ERR_DUPLICATE;
                }
                gen_context_set_error(
                    ctx, rc, node->line, node->column, node->offset, "failed to store property"
                );
                return rc;
            }
        }
        break;
    default:
        gen_context_set_error(
            ctx, GEN_ERR_SEMANTIC, node->line, node->column, node->offset, "invalid value expression"
        );
        return GEN_ERR_SEMANTIC;
    }
    if (v == NULL) {
        gen_context_set_error(
            ctx, GEN_ERR_OUT_OF_MEMORY, node->line, node->column, node->offset, "out of memory"
        );
        return GEN_ERR_OUT_OF_MEMORY;
    }
    *out_value = v;
    return GEN_OK;
}

static GenResult gen_add_type(
    GenContext *ctx,
    GenDocument *doc,
    const GenAst *node,
    GenTypeKind kind
)
{
    GenTypeNode *existing;
    GenTypeNode *type;

    ctx->source_path = node->origin;
    HASH_FIND_STR(doc->types, node->u.type_decl.name, existing);
    if (existing != NULL) {
        gen_context_set_error(
            ctx,
            GEN_ERR_DUPLICATE,
            node->line,
            node->column,
            node->offset,
            "duplicate type '%s'",
            node->u.type_decl.name
        );
        return GEN_ERR_DUPLICATE;
    }
    type = (GenTypeNode *)gen_calloc(1, sizeof(GenTypeNode));
    if (type == NULL) {
        gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, node->line, node->column, node->offset, "out of memory");
        return GEN_ERR_OUT_OF_MEMORY;
    }
    type->name = gen_strdup(node->u.type_decl.name);
    type->kind = kind;
    type->decl_index = doc->types_order.count;
    type->line = node->line;
    type->column = node->column;
    type->source_path = node->origin != NULL ? gen_strdup(node->origin) : NULL;
    if (node->origin != NULL && type->source_path == NULL) {
        gen_free(type->name);
        gen_free(type);
        gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, node->line, node->column, node->offset, "out of memory");
        return GEN_ERR_OUT_OF_MEMORY;
    }
    gen_ptrvec_init(&type->children);
    if (type->name == NULL) {
        gen_free(type->source_path);
        gen_free(type);
        gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, node->line, node->column, node->offset, "out of memory");
        return GEN_ERR_OUT_OF_MEMORY;
    }
    if (node->u.type_decl.parent != NULL) {
        type->parent_name = gen_strdup(node->u.type_decl.parent);
        if (type->parent_name == NULL) {
            gen_free(type->name);
            gen_free(type->source_path);
            gen_free(type);
            gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, node->line, node->column, node->offset, "out of memory");
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }
    if (node->u.type_decl.props != NULL) {
        GenResult rc = gen_ast_to_value(ctx, node->u.type_decl.props, 0, &type->props);
        if (rc != GEN_OK) {
            gen_free(type->name);
            gen_free(type->parent_name);
            gen_free(type->source_path);
            gen_free(type);
            return rc;
        }
    }
    HASH_ADD_KEYPTR(hh, doc->types, type->name, strlen(type->name), type);
    if (!gen_ptrvec_push(&doc->types_order, type)) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    return GEN_OK;
}

static GenResult gen_walk_refs(GenContext *ctx, GenDocument *doc, const GenValue *value, const char *owner)
{
    size_t i;
    GenResult rc;

    if (value == NULL) {
        return GEN_OK;
    }
    switch (value->kind) {
    case GEN_VALUE_REFERENCE:
        if (gen_entity_lookup(doc, value->u.reference.data) == NULL) {
            gen_context_set_error(
                ctx,
                GEN_ERR_SEMANTIC,
                0,
                0,
                0,
                "unknown reference '@%s'",
                value->u.reference.data
            );
            break;
        }
        rc = gen_relation_add(doc, GEN_REL_REFERENCES, owner, value->u.reference.data);
        if (rc != GEN_OK) {
            return rc;
        }
        break;
    case GEN_VALUE_LIST:
        for (i = 0; i < value->u.list.count; i++) {
            rc = gen_walk_refs(ctx, doc, value->u.list.items[i], owner);
            if (rc == GEN_ERR_OUT_OF_MEMORY) {
                return rc;
            }
            rc = gen_relation_add(doc, GEN_REL_CONTAINS, owner, owner);
            if (rc != GEN_OK) {
                return rc;
            }
        }
        break;
    case GEN_VALUE_OBJECT:
        for (i = 0; i < value->u.object.count; i++) {
            rc = gen_relation_add(doc, GEN_REL_HAS_PROPERTY, owner, value->u.object.keys[i]);
            if (rc != GEN_OK) {
                return rc;
            }
            rc = gen_walk_refs(ctx, doc, value->u.object.values[i], owner);
            if (rc == GEN_ERR_OUT_OF_MEMORY) {
                return rc;
            }
        }
        break;
    default:
        break;
    }
    return GEN_OK;
}

static GenResult gen_cycle_visit(
    GenContext *ctx,
    GenDocument *doc,
    GenTypeNode *node,
    GenColorEntry **colors
)
{
    GenColorEntry *entry;
    GenResult rc;

    HASH_FIND_STR(*colors, node->name, entry);
    ctx->source_path = node->source_path;
    if (entry != NULL && entry->color == GEN_COLOR_GRAY) {
        gen_context_set_error(
            ctx,
            GEN_ERR_CYCLE,
            node->line,
            node->column,
            0,
            "type cycle detected involving '%s'",
            node->name
        );
        return GEN_ERR_CYCLE;
    }
    if (entry != NULL && entry->color == GEN_COLOR_BLACK) {
        return GEN_OK;
    }
    if (entry == NULL) {
        entry = (GenColorEntry *)gen_calloc(1, sizeof(GenColorEntry));
        if (entry == NULL) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        entry->name = gen_strdup(node->name);
        if (entry->name == NULL) {
            gen_free(entry);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        HASH_ADD_KEYPTR(hh, *colors, entry->name, strlen(entry->name), entry);
    }
    entry->color = GEN_COLOR_GRAY;
    if (node->parent != NULL) {
        rc = gen_cycle_visit(ctx, doc, node->parent, colors);
        if (rc != GEN_OK) {
            return rc;
        }
    }
    entry->color = GEN_COLOR_BLACK;
    return GEN_OK;
}

static const char *gen_kind_name(GenValueType kind)
{
    switch (kind) {
    case GEN_VALUE_NULL:
        return "null";
    case GEN_VALUE_BOOL:
        return "bool";
    case GEN_VALUE_INT:
        return "int";
    case GEN_VALUE_FLOAT:
        return "float";
    case GEN_VALUE_STRING:
        return "string";
    case GEN_VALUE_LIST:
        return "list";
    case GEN_VALUE_OBJECT:
        return "object";
    case GEN_VALUE_REFERENCE:
        return "reference";
    default:
        return "value";
    }
}

static int gen_type_has_schema(const GenTypeNode *type)
{
    while (type != NULL) {
        if (type->props != NULL && type->props->kind == GEN_VALUE_OBJECT &&
            type->props->u.object.count > 0) {
            return 1;
        }
        type = type->parent;
    }
    return 0;
}

static const GenValue *gen_schema_lookup(const GenTypeNode *type, const char *key)
{
    while (type != NULL) {
        if (type->props != NULL && type->props->kind == GEN_VALUE_OBJECT) {
            const GenValue *item = NULL;
            if (gen_value_object_get(type->props, key, &item) == GEN_OK) {
                return item;
            }
        }
        type = type->parent;
    }
    return NULL;
}

static GenResult gen_check_shape(
    GenContext *ctx,
    const GenEntity *ent,
    const char *type_name,
    const char *where,
    const GenValue *schema,
    const GenValue *value
)
{
    size_t i;
    GenResult rc;

    if (schema->kind == GEN_VALUE_OBJECT) {
        if (value->kind != GEN_VALUE_OBJECT) {
            gen_context_set_error(
                ctx,
                GEN_ERR_SEMANTIC,
                ent->line,
                ent->column,
                0,
                "entity '%s' field '%s' must be an object to match type '%s'",
                ent->name,
                where[0] != '\0' ? where : "<root>",
                type_name
            );
            return GEN_ERR_SEMANTIC;
        }
        for (i = 0; i < schema->u.object.count; i++) {
            const char *key = schema->u.object.keys[i];
            const GenValue *child = NULL;
            char child_where[256];
            if (gen_value_object_get(value, key, &child) != GEN_OK) {
                gen_context_set_error(
                    ctx,
                    GEN_ERR_SEMANTIC,
                    ent->line,
                    ent->column,
                    0,
                    "entity '%s' is missing property '%s%s%s' required by type '%s'",
                    ent->name,
                    where,
                    where[0] != '\0' ? "." : "",
                    key,
                    type_name
                );
                continue;
            }
            if (where[0] == '\0') {
                snprintf(child_where, sizeof(child_where), "%s", key);
            } else {
                snprintf(child_where, sizeof(child_where), "%s.%s", where, key);
            }
            rc = gen_check_shape(
                ctx, ent, type_name, child_where, schema->u.object.values[i], child
            );
            if (rc == GEN_ERR_OUT_OF_MEMORY) {
                return rc;
            }
        }
        return GEN_OK;
    }
    if (schema->kind == GEN_VALUE_LIST) {
        if (value->kind != GEN_VALUE_LIST) {
            gen_context_set_error(
                ctx,
                GEN_ERR_SEMANTIC,
                ent->line,
                ent->column,
                0,
                "entity '%s' field '%s' must be a list to match type '%s'",
                ent->name,
                where[0] != '\0' ? where : "<root>",
                type_name
            );
            return GEN_ERR_SEMANTIC;
        }
        if (schema->u.list.count > 0) {
            for (i = 0; i < value->u.list.count; i++) {
                char item_where[256];
                snprintf(item_where, sizeof(item_where), "%s[%zu]", where, i);
                rc = gen_check_shape(
                    ctx, ent, type_name, item_where, schema->u.list.items[0], value->u.list.items[i]
                );
                if (rc == GEN_ERR_OUT_OF_MEMORY) {
                    return rc;
                }
            }
        }
        return GEN_OK;
    }
    if (value->kind != schema->kind) {
        gen_context_set_error(
            ctx,
            GEN_ERR_SEMANTIC,
            ent->line,
            ent->column,
            0,
            "entity '%s' property '%s' has type %s, type '%s' requires %s",
            ent->name,
            where,
            gen_kind_name(value->kind),
            type_name,
            gen_kind_name(schema->kind)
        );
        return GEN_ERR_SEMANTIC;
    }
    return GEN_OK;
}

static GenResult gen_check_entity_schema(GenContext *ctx, GenEntity *ent)
{
    GenStrVec keys;
    size_t i;
    GenResult rc;

    if (ent->type == NULL || !gen_type_has_schema(ent->type)) {
        return GEN_OK;
    }
    if (ent->value == NULL || ent->value->kind != GEN_VALUE_OBJECT) {
        gen_context_set_error(
            ctx,
            GEN_ERR_SEMANTIC,
            ent->line,
            ent->column,
            0,
            "entity '%s' must be an object to match type '%s'",
            ent->name,
            ent->type->name
        );
        return GEN_ERR_SEMANTIC;
    }

    gen_strvec_init(&keys);
    rc = GEN_OK;
    {
        const GenTypeNode *t = ent->type;
        while (t != NULL) {
            if (t->props != NULL && t->props->kind == GEN_VALUE_OBJECT) {
                for (i = 0; i < t->props->u.object.count; i++) {
                    if (!gen_strvec_contains(&keys, t->props->u.object.keys[i]) &&
                        !gen_strvec_push_copy(&keys, t->props->u.object.keys[i])) {
                        gen_strvec_free_all(&keys);
                        return GEN_ERR_OUT_OF_MEMORY;
                    }
                }
            }
            t = t->parent;
        }
    }
    for (i = 0; i < keys.count; i++) {
        const GenValue *schema = gen_schema_lookup(ent->type, keys.items[i]);
        const GenValue *got = NULL;
        if (schema == NULL) {
            continue;
        }
        if (gen_value_object_get(ent->value, keys.items[i], &got) != GEN_OK) {
            gen_context_set_error(
                ctx,
                GEN_ERR_SEMANTIC,
                ent->line,
                ent->column,
                0,
                "entity '%s' is missing property '%s' required by type '%s'",
                ent->name,
                keys.items[i],
                ent->type->name
            );
            rc = GEN_ERR_SEMANTIC;
            continue;
        }
        if (gen_check_shape(ctx, ent, ent->type->name, keys.items[i], schema, got) ==
            GEN_ERR_OUT_OF_MEMORY) {
            gen_strvec_free_all(&keys);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (ctx->last_error.code != GEN_OK) {
            rc = ctx->last_error.code;
        }
    }
    gen_strvec_free_all(&keys);
    return rc == GEN_ERR_OUT_OF_MEMORY ? rc : GEN_OK;
}

GenResult gen_semantic_analyze(GenContext *ctx, const GenAst *program, GenDocument *doc)
{
    size_t i;
    GenResult rc;
    GenColorEntry *colors = NULL;

    if (ctx == NULL || program == NULL || doc == NULL || program->kind != GEN_AST_PROGRAM) {
        if (ctx != NULL) {
            gen_context_set_error(ctx, GEN_ERR_INVALID_ARGUMENT, 0, 0, 0, "invalid semantic input");
        }
        return GEN_ERR_INVALID_ARGUMENT;
    }

    for (i = 0; i < program->u.program.count; i++) {
        const GenAst *stmt = program->u.program.items[i];
        ctx->source_path = stmt->origin;
        if (stmt->kind == GEN_AST_GENUS) {
            rc = gen_add_type(ctx, doc, stmt, GEN_TYPE_KIND_GENUS);
            if (rc == GEN_ERR_OUT_OF_MEMORY) {
                return rc;
            }
        } else if (stmt->kind == GEN_AST_SPECIES) {
            rc = gen_add_type(ctx, doc, stmt, GEN_TYPE_KIND_SPECIES);
            if (rc == GEN_ERR_OUT_OF_MEMORY) {
                return rc;
            }
        } else if (stmt->kind == GEN_AST_IMPORT) {
            gen_context_set_error(
                ctx,
                GEN_ERR_SEMANTIC,
                stmt->line,
                stmt->column,
                stmt->offset,
                "unresolved iceaktar"
            );
        } else if (stmt->kind == GEN_AST_SET) {
            GenSetNode *existing;
            GenSetNode *set;
            HASH_FIND_STR(doc->sets, stmt->u.set_decl.name, existing);
            if (existing != NULL) {
                gen_context_set_error(
                    ctx,
                    GEN_ERR_DUPLICATE,
                    stmt->line,
                    stmt->column,
                    stmt->offset,
                    "duplicate set '%s'",
                    stmt->u.set_decl.name
                );
                continue;
            }
            set = (GenSetNode *)gen_calloc(1, sizeof(GenSetNode));
            if (set == NULL) {
                gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, stmt->line, stmt->column, stmt->offset, "out of memory");
                return GEN_ERR_OUT_OF_MEMORY;
            }
            set->name = gen_strdup(stmt->u.set_decl.name);
            set->decl_index = doc->sets_order.count;
            set->line = stmt->line;
            set->column = stmt->column;
            set->source_path = stmt->origin != NULL ? gen_strdup(stmt->origin) : NULL;
            if (stmt->origin != NULL && set->source_path == NULL) {
                gen_free(set->name);
                gen_free(set);
                return GEN_ERR_OUT_OF_MEMORY;
            }
            gen_ptrvec_init(&set->members);
            if (set->name == NULL) {
                gen_free(set->source_path);
                gen_free(set);
                return GEN_ERR_OUT_OF_MEMORY;
            }
            if (stmt->u.set_decl.props != NULL) {
                rc = gen_ast_to_value(ctx, stmt->u.set_decl.props, 0, &set->props);
                if (rc != GEN_OK) {
                    gen_free(set->name);
                    gen_free(set->source_path);
                    gen_free(set);
                    return rc;
                }
            }
            HASH_ADD_KEYPTR(hh, doc->sets, set->name, strlen(set->name), set);
            if (!gen_ptrvec_push(&doc->sets_order, set)) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
        }
    }

    for (i = 0; i < doc->types_order.count; i++) {
        GenTypeNode *type = (GenTypeNode *)doc->types_order.items[i];
        ctx->source_path = type->source_path;
        if (type->parent_name != NULL) {
            GenTypeNode *parent = gen_type_lookup(doc, type->parent_name);
            if (parent == NULL) {
                gen_context_set_error(
                    ctx,
                    GEN_ERR_SEMANTIC,
                    type->line,
                    type->column,
                    0,
                    "unknown parent type '%s'",
                    type->parent_name
                );
                continue;
            }
            type->parent = parent;
            if (!gen_ptrvec_push(&parent->children, type)) {
                return GEN_ERR_OUT_OF_MEMORY;
            }
            rc = gen_relation_add(doc, GEN_REL_SUBTYPE_OF, type->name, parent->name);
            if (rc != GEN_OK) {
                return rc;
            }
        }
    }

    for (i = 0; i < doc->types_order.count; i++) {
        GenTypeNode *type = (GenTypeNode *)doc->types_order.items[i];
        rc = gen_cycle_visit(ctx, doc, type, &colors);
        if (rc == GEN_ERR_OUT_OF_MEMORY) {
            gen_colors_free(colors);
            return rc;
        }
    }
    gen_colors_free(colors);

    for (i = 0; i < program->u.program.count; i++) {
        const GenAst *stmt = program->u.program.items[i];
        GenEntity *existing;
        GenEntity *ent;
        if (stmt->kind != GEN_AST_DATA) {
            continue;
        }
        ctx->source_path = stmt->origin;
        HASH_FIND_STR(doc->entities, stmt->u.data_decl.name, existing);
        if (existing != NULL) {
            gen_context_set_error(
                ctx,
                GEN_ERR_DUPLICATE,
                stmt->line,
                stmt->column,
                stmt->offset,
                "duplicate entity '%s'",
                stmt->u.data_decl.name
            );
            continue;
        }
        ent = (GenEntity *)gen_calloc(1, sizeof(GenEntity));
        if (ent == NULL) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        ent->name = gen_strdup(stmt->u.data_decl.name);
        ent->decl_index = doc->entities_order.count;
        ent->line = stmt->line;
        ent->column = stmt->column;
        ent->source_path = stmt->origin != NULL ? gen_strdup(stmt->origin) : NULL;
        if (stmt->origin != NULL && ent->source_path == NULL) {
            gen_free(ent->name);
            gen_free(ent);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        gen_ptrvec_init(&ent->sets);
        if (ent->name == NULL) {
            gen_free(ent->source_path);
            gen_free(ent);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (stmt->u.data_decl.type != NULL) {
            ent->type_name = gen_strdup(stmt->u.data_decl.type);
            if (ent->type_name == NULL) {
                gen_free(ent->name);
                gen_free(ent->source_path);
                gen_free(ent);
                return GEN_ERR_OUT_OF_MEMORY;
            }
            ent->type = gen_type_lookup(doc, ent->type_name);
            if (ent->type == NULL) {
                gen_context_set_error(
                    ctx,
                    GEN_ERR_SEMANTIC,
                    stmt->line,
                    stmt->column,
                    stmt->offset,
                    "unknown type '%s'",
                    ent->type_name
                );
                gen_free(ent->name);
                gen_free(ent->type_name);
                gen_free(ent->source_path);
                gen_free(ent);
                continue;
            }
            rc = gen_relation_add(doc, GEN_REL_TYPE_OF, ent->name, ent->type_name);
            if (rc != GEN_OK) {
                gen_free(ent->name);
                gen_free(ent->type_name);
                gen_free(ent->source_path);
                gen_free(ent);
                return rc;
            }
        }
        rc = gen_ast_to_value(ctx, stmt->u.data_decl.value, 0, &ent->value);
        if (rc != GEN_OK) {
            gen_free(ent->name);
            gen_free(ent->type_name);
            gen_free(ent->source_path);
            gen_free(ent);
            return rc;
        }
        HASH_ADD_KEYPTR(hh, doc->entities, ent->name, strlen(ent->name), ent);
        if (!gen_ptrvec_push(&doc->entities_order, ent)) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }

    for (i = 0; i < doc->entities_order.count; i++) {
        GenEntity *ent = (GenEntity *)doc->entities_order.items[i];
        ctx->source_path = ent->source_path;
        rc = gen_check_entity_schema(ctx, ent);
        if (rc == GEN_ERR_OUT_OF_MEMORY) {
            return rc;
        }
        rc = gen_walk_refs(ctx, doc, ent->value, ent->name);
        if (rc == GEN_ERR_OUT_OF_MEMORY) {
            return rc;
        }
    }

    for (i = 0; i < program->u.program.count; i++) {
        const GenAst *stmt = program->u.program.items[i];
        GenEntity *ent;
        GenSetNode *set;
        GenMembershipRec *rec;
        size_t j;
        if (stmt->kind != GEN_AST_MEMBERSHIP) {
            continue;
        }
        ctx->source_path = stmt->origin;
        ent = gen_entity_lookup(doc, stmt->u.membership.entity);
        if (ent == NULL) {
            gen_context_set_error(
                ctx,
                GEN_ERR_SEMANTIC,
                stmt->line,
                stmt->column,
                stmt->offset,
                "unknown entity '%s' in membership",
                stmt->u.membership.entity
            );
            continue;
        }
        set = gen_set_lookup(doc, stmt->u.membership.set);
        if (set == NULL) {
            gen_context_set_error(
                ctx,
                GEN_ERR_SEMANTIC,
                stmt->line,
                stmt->column,
                stmt->offset,
                "unknown set '%s'",
                stmt->u.membership.set
            );
            continue;
        }
        {
            int already = 0;
            for (j = 0; j < ent->sets.count; j++) {
                if (ent->sets.items[j] == set) {
                    gen_context_set_error(
                        ctx,
                        GEN_ERR_DUPLICATE,
                        stmt->line,
                        stmt->column,
                        stmt->offset,
                        "'%s' is already a member of '%s'",
                        ent->name,
                        set->name
                    );
                    already = 1;
                    break;
                }
            }
            if (already) {
                continue;
            }
        }
        if (!gen_ptrvec_push(&ent->sets, set) || !gen_ptrvec_push(&set->members, ent)) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rec = (GenMembershipRec *)gen_calloc(1, sizeof(GenMembershipRec));
        if (rec == NULL) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rec->entity = gen_strdup(ent->name);
        rec->set = gen_strdup(set->name);
        if (rec->entity == NULL || rec->set == NULL) {
            gen_free(rec->entity);
            gen_free(rec->set);
            gen_free(rec);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (!gen_ptrvec_push(&doc->memberships, rec)) {
            gen_free(rec->entity);
            gen_free(rec->set);
            gen_free(rec);
            return GEN_ERR_OUT_OF_MEMORY;
        }
        rc = gen_relation_add(doc, GEN_REL_MEMBER_OF, ent->name, set->name);
        if (rc != GEN_OK) {
            return rc;
        }
    }

    return ctx->last_error.code;
}
