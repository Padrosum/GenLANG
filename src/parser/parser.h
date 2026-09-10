#ifndef GENLANG_PARSER_H
#define GENLANG_PARSER_H

#include "ast/ast.h"

typedef enum {
    GEN_PARSE_DOCUMENT = 0,
    GEN_PARSE_REPL
} GenParseMode;

GenResult gen_parse(
    GenContext *ctx,
    const char *source,
    size_t length,
    GenParseMode mode,
    const char *origin_path,
    GenAstProgram *out_program
);

#endif /* GENLANG_PARSER_H */
