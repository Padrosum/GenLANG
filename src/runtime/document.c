#include "runtime/runtime.h"

#include "error/error.h"
#include "parser/parser.h"
#include "semantic/import.h"
#include "semantic/semantic.h"
#include "serializer/serializer.h"

GenDocument *gen_document_new(void)
{
    GenDocument *doc = (GenDocument *)gen_calloc(1, sizeof(GenDocument));
    if (doc == NULL) {
        return NULL;
    }
    gen_ptrvec_init(&doc->types_order);
    gen_ptrvec_init(&doc->sets_order);
    gen_ptrvec_init(&doc->entities_order);
    gen_ptrvec_init(&doc->relations);
    gen_ptrvec_init(&doc->memberships);
    return doc;
}

static void gen_type_node_free(GenTypeNode *node)
{
    if (node == NULL) {
        return;
    }
    gen_free(node->name);
    gen_free(node->parent_name);
    gen_free(node->source_path);
    gen_value_destroy(node->props);
    gen_ptrvec_free(&node->children);
    gen_free(node);
}

static void gen_set_node_free(GenSetNode *node)
{
    if (node == NULL) {
        return;
    }
    gen_free(node->name);
    gen_value_destroy(node->props);
    gen_ptrvec_free(&node->members);
    gen_free(node->source_path);
    gen_free(node);
}

static void gen_entity_free(GenEntity *node)
{
    if (node == NULL) {
        return;
    }
    gen_free(node->name);
    gen_free(node->type_name);
    gen_value_destroy(node->value);
    gen_ptrvec_free(&node->sets);
    gen_free(node->source_path);
    gen_free(node);
}

void gen_document_destroy(GenDocument *doc)
{
    GenTypeNode *type;
    GenTypeNode *type_tmp;
    GenSetNode *set;
    GenSetNode *set_tmp;
    GenEntity *ent;
    GenEntity *ent_tmp;
    size_t i;

    if (doc == NULL) {
        return;
    }
    HASH_ITER(hh, doc->types, type, type_tmp) {
        HASH_DEL(doc->types, type);
        gen_type_node_free(type);
    }
    HASH_ITER(hh, doc->sets, set, set_tmp) {
        HASH_DEL(doc->sets, set);
        gen_set_node_free(set);
    }
    HASH_ITER(hh, doc->entities, ent, ent_tmp) {
        HASH_DEL(doc->entities, ent);
        gen_entity_free(ent);
    }
    for (i = 0; i < doc->relations.count; i++) {
        GenRelationRec *rel = (GenRelationRec *)doc->relations.items[i];
        gen_free(rel->from);
        gen_free(rel->to);
        gen_free(rel);
    }
    for (i = 0; i < doc->memberships.count; i++) {
        GenMembershipRec *m = (GenMembershipRec *)doc->memberships.items[i];
        gen_free(m->entity);
        gen_free(m->set);
        gen_free(m);
    }
    gen_ptrvec_free(&doc->types_order);
    gen_ptrvec_free(&doc->sets_order);
    gen_ptrvec_free(&doc->entities_order);
    gen_ptrvec_free(&doc->relations);
    gen_ptrvec_free(&doc->memberships);
    gen_free(doc);
}

void gen_document_free(GenDocument *document)
{
    gen_document_destroy(document);
}

static GenResult gen_parse_to_document(
    GenContext *ctx,
    const char *source,
    size_t length,
    const char *origin_path,
    GenDocument **out_document
)
{
    GenExpandedAst expanded;
    GenResult rc;
    GenDocument *doc;

    if (ctx == NULL || source == NULL || out_document == NULL) {
        if (ctx != NULL) {
            gen_context_set_error(ctx, GEN_ERR_INVALID_ARGUMENT, 0, 0, 0, "invalid argument");
        }
        return GEN_ERR_INVALID_ARGUMENT;
    }
    *out_document = NULL;
    gen_context_clear_error(ctx);
    rc = gen_expand_document(ctx, source, length, origin_path, &expanded);
    if (rc != GEN_OK) {
        return rc;
    }
    doc = gen_document_new();
    if (doc == NULL) {
        gen_expanded_ast_free(&expanded);
        gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
        return GEN_ERR_OUT_OF_MEMORY;
    }
    rc = gen_semantic_analyze(ctx, expanded.root, doc);
    gen_expanded_ast_free(&expanded);
    if (rc != GEN_OK) {
        gen_document_destroy(doc);
        return rc;
    }
    *out_document = doc;
    return GEN_OK;
}

GenResult gen_document_parse(GenContext *ctx, const char *source, GenDocument **out_document)
{
    if (source == NULL) {
        if (ctx != NULL) {
            gen_context_set_error(ctx, GEN_ERR_INVALID_ARGUMENT, 0, 0, 0, "source is NULL");
        }
        return GEN_ERR_INVALID_ARGUMENT;
    }
    return gen_parse_to_document(ctx, source, strlen(source), NULL, out_document);
}

GenResult gen_document_parse_at(
    GenContext *ctx,
    const char *source,
    const char *origin_path,
    GenDocument **out_document
)
{
    if (source == NULL) {
        if (ctx != NULL) {
            gen_context_set_error(ctx, GEN_ERR_INVALID_ARGUMENT, 0, 0, 0, "source is NULL");
        }
        return GEN_ERR_INVALID_ARGUMENT;
    }
    return gen_parse_to_document(ctx, source, strlen(source), origin_path, out_document);
}

GenResult gen_document_parse_n(
    GenContext *ctx,
    const char *source,
    size_t length,
    GenDocument **out_document
)
{
    return gen_parse_to_document(ctx, source, length, NULL, out_document);
}

size_t gen_type_count(const GenDocument *document)
{
    return document != NULL ? document->types_order.count : 0;
}

GenResult gen_type_name(const GenDocument *document, size_t index, const char **out_name)
{
    GenTypeNode *node;
    if (document == NULL || out_name == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    if (index >= document->types_order.count) {
        return GEN_ERR_INDEX;
    }
    node = (GenTypeNode *)document->types_order.items[index];
    *out_name = node->name;
    return GEN_OK;
}

GenResult gen_type_kind(const GenDocument *document, size_t index, GenTypeKind *out_kind)
{
    GenTypeNode *node;
    if (document == NULL || out_kind == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    if (index >= document->types_order.count) {
        return GEN_ERR_INDEX;
    }
    node = (GenTypeNode *)document->types_order.items[index];
    *out_kind = node->kind;
    return GEN_OK;
}

GenResult gen_type_parent(const GenDocument *document, const char *name, const char **out_parent)
{
    GenTypeNode *node;
    if (document == NULL || name == NULL || out_parent == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    node = gen_type_lookup(document, name);
    if (node == NULL) {
        return GEN_ERR_NOT_FOUND;
    }
    *out_parent = node->parent != NULL ? node->parent->name : NULL;
    return GEN_OK;
}

GenResult gen_type_location(
    const GenDocument *document,
    const char *name,
    size_t *out_line,
    size_t *out_column
)
{
    GenTypeNode *node;
    if (document == NULL || name == NULL || out_line == NULL || out_column == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    node = gen_type_lookup(document, name);
    if (node == NULL) {
        return GEN_ERR_NOT_FOUND;
    }
    *out_line = node->line;
    *out_column = node->column;
    return GEN_OK;
}

GenResult gen_type_properties(
    const GenDocument *document,
    const char *name,
    const GenValue **out_value
)
{
    GenTypeNode *node;
    if (document == NULL || name == NULL || out_value == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    node = gen_type_lookup(document, name);
    if (node == NULL) {
        return GEN_ERR_NOT_FOUND;
    }
    *out_value = node->props;
    return GEN_OK;
}

GenResult gen_type_property(
    const GenDocument *document,
    const char *type_name,
    const char *key,
    const GenValue **out_value
)
{
    const GenValue *props = NULL;
    GenResult rc = gen_type_properties(document, type_name, &props);
    if (rc != GEN_OK) {
        return rc;
    }
    if (props == NULL) {
        return GEN_ERR_NOT_FOUND;
    }
    return gen_value_object_get(props, key, out_value);
}

size_t gen_set_count(const GenDocument *document)
{
    return document != NULL ? document->sets_order.count : 0;
}

GenResult gen_set_name(const GenDocument *document, size_t index, const char **out_name)
{
    GenSetNode *node;
    if (document == NULL || out_name == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    if (index >= document->sets_order.count) {
        return GEN_ERR_INDEX;
    }
    node = (GenSetNode *)document->sets_order.items[index];
    *out_name = node->name;
    return GEN_OK;
}

GenResult gen_set_location(
    const GenDocument *document,
    const char *name,
    size_t *out_line,
    size_t *out_column
)
{
    GenSetNode *node;
    if (document == NULL || name == NULL || out_line == NULL || out_column == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    node = gen_set_lookup(document, name);
    if (node == NULL) {
        return GEN_ERR_NOT_FOUND;
    }
    *out_line = node->line;
    *out_column = node->column;
    return GEN_OK;
}

GenResult gen_set_properties(
    const GenDocument *document,
    const char *name,
    const GenValue **out_value
)
{
    GenSetNode *node;
    if (document == NULL || name == NULL || out_value == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    node = gen_set_lookup(document, name);
    if (node == NULL) {
        return GEN_ERR_NOT_FOUND;
    }
    *out_value = node->props;
    return GEN_OK;
}

GenResult gen_set_property(
    const GenDocument *document,
    const char *set_name,
    const char *key,
    const GenValue **out_value
)
{
    const GenValue *props = NULL;
    GenResult rc = gen_set_properties(document, set_name, &props);
    if (rc != GEN_OK) {
        return rc;
    }
    if (props == NULL) {
        return GEN_ERR_NOT_FOUND;
    }
    return gen_value_object_get(props, key, out_value);
}

size_t gen_entity_count(const GenDocument *document)
{
    return document != NULL ? document->entities_order.count : 0;
}

GenResult gen_entity_name(const GenDocument *document, size_t index, const char **out_name)
{
    GenEntity *node;
    if (document == NULL || out_name == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    if (index >= document->entities_order.count) {
        return GEN_ERR_INDEX;
    }
    node = (GenEntity *)document->entities_order.items[index];
    *out_name = node->name;
    return GEN_OK;
}

GenResult gen_entity_type_name(
    const GenDocument *document,
    const char *name,
    const char **out_type
)
{
    GenEntity *ent;
    if (document == NULL || name == NULL || out_type == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    ent = gen_entity_lookup(document, name);
    if (ent == NULL) {
        return GEN_ERR_NOT_FOUND;
    }
    *out_type = ent->type != NULL ? ent->type->name : NULL;
    return GEN_OK;
}

GenResult gen_entity_location(
    const GenDocument *document,
    const char *name,
    size_t *out_line,
    size_t *out_column
)
{
    GenEntity *ent;
    if (document == NULL || name == NULL || out_line == NULL || out_column == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    ent = gen_entity_lookup(document, name);
    if (ent == NULL) {
        return GEN_ERR_NOT_FOUND;
    }
    *out_line = ent->line;
    *out_column = ent->column;
    return GEN_OK;
}

GenResult gen_entity_value(
    const GenDocument *document,
    const char *name,
    const GenValue **out_value
)
{
    GenEntity *ent;
    if (document == NULL || name == NULL || out_value == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    ent = gen_entity_lookup(document, name);
    if (ent == NULL) {
        return GEN_ERR_NOT_FOUND;
    }
    *out_value = ent->value;
    return GEN_OK;
}
