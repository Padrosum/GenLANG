#include "runtime/runtime.h"

GenSetNode *gen_set_lookup(const GenDocument *doc, const char *name)
{
    GenSetNode *node = NULL;
    if (doc == NULL || name == NULL) {
        return NULL;
    }
    HASH_FIND_STR(doc->sets, name, node);
    return node;
}

GenEntity *gen_entity_lookup(const GenDocument *doc, const char *name)
{
    GenEntity *node = NULL;
    if (doc == NULL || name == NULL) {
        return NULL;
    }
    HASH_FIND_STR(doc->entities, name, node);
    return node;
}
