#include "runtime/runtime.h"

GenResult gen_relation_add(
    GenDocument *doc,
    GenRelationType kind,
    const char *from,
    const char *to
)
{
    GenRelationRec *rel;

    if (doc == NULL || from == NULL || to == NULL) {
        return GEN_ERR_INVALID_ARGUMENT;
    }
    rel = (GenRelationRec *)gen_calloc(1, sizeof(GenRelationRec));
    if (rel == NULL) {
        return GEN_ERR_OUT_OF_MEMORY;
    }
    rel->kind = kind;
    rel->from = gen_strdup(from);
    rel->to = gen_strdup(to);
    if (rel->from == NULL || rel->to == NULL) {
        gen_free(rel->from);
        gen_free(rel->to);
        gen_free(rel);
        return GEN_ERR_OUT_OF_MEMORY;
    }
    if (!gen_ptrvec_push(&doc->relations, rel)) {
        gen_free(rel->from);
        gen_free(rel->to);
        gen_free(rel);
        return GEN_ERR_OUT_OF_MEMORY;
    }
    return GEN_OK;
}
