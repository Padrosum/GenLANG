#include "ast/ast.h"

GenAst *gen_ast_new(GenArena *arena, GenAstKind kind, const GenToken *tok)
{
    GenAst *node = (GenAst *)gen_arena_alloc(arena, sizeof(GenAst));
    if (node == NULL) {
        return NULL;
    }
    memset(node, 0, sizeof(GenAst));
    node->kind = kind;
    if (tok != NULL) {
        node->line = tok->line;
        node->column = tok->column;
        node->offset = tok->offset;
    }
    return node;
}

void gen_ast_program_free(GenAstProgram *program)
{
    if (program == NULL) {
        return;
    }
    gen_arena_destroy(program->arena);
    program->arena = NULL;
    program->root = NULL;
}
