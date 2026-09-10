#ifndef GENLANG_IMPORT_H
#define GENLANG_IMPORT_H

#include "ast/ast.h"
#include "internal/vector.h"

typedef struct {
    GenPtrVec arenas;
    GenAst *root;
} GenExpandedAst;

void gen_expanded_ast_free(GenExpandedAst *expanded);

GenResult gen_expand_document(
    GenContext *ctx,
    const char *source,
    size_t length,
    const char *origin_path,
    GenExpandedAst *out_expanded
);

#endif /* GENLANG_IMPORT_H */
