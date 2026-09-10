#ifndef GENLANG_SEMANTIC_H
#define GENLANG_SEMANTIC_H

#include "ast/ast.h"
#include "runtime/runtime.h"

GenResult gen_semantic_analyze(GenContext *ctx, const GenAst *program, GenDocument *doc);

#endif /* GENLANG_SEMANTIC_H */
