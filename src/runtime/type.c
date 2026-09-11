#include "runtime/runtime.h"

GenTypeNode *gen_type_lookup(const GenDocument *doc, const char *name)
{
    GenTypeNode *node = NULL;
    if (doc == NULL || name == NULL) {
        return NULL;
    }
    HASH_FIND_STR(doc->types, name, node);
    return node;
}

bool gen_type_is_or_subtype(const GenTypeNode *type, const GenTypeNode *ancestor)
{
    const GenTypeNode *cur;

    if (type == NULL || ancestor == NULL) {
        return false;
    }
    cur = type;
    while (cur != NULL) {
        if (cur == ancestor) {
            return true;
        }
        cur = cur->parent;
    }
    return false;
}

GenResult gen_type_collect_ancestors(const GenTypeNode *type, GenStrVec *out)
{
    const GenTypeNode *cur;

    if (type == NULL || out == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    cur = type->parent;
    while (cur != NULL) {
        if (!gen_strvec_push_copy(out, cur->name)) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        cur = cur->parent;
    }
    return GEN_OK;
}

static GenResult gen_type_collect_descendants_rec(const GenTypeNode *type, GenStrVec *out)
{
    size_t i;
    for (i = 0; i < type->children.count; i++) {
        GenTypeNode *child = (GenTypeNode *)type->children.items[i];
        if (!gen_strvec_push_copy(out, child->name)) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (gen_type_collect_descendants_rec(child, out) != GEN_OK) {
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }
    return GEN_OK;
}

GenResult gen_type_collect_descendants(const GenTypeNode *type, GenStrVec *out)
{
    if (type == NULL || out == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    return gen_type_collect_descendants_rec(type, out);
}
