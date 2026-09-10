#ifndef GENLANG_LEXER_H
#define GENLANG_LEXER_H

#include "internal/common.h"

typedef enum GenTokenKind {
    TOK_EOF = 0,
    TOK_IDENT,
    TOK_STRING,
    TOK_INT,
    TOK_FLOAT,
    TOK_TRUE,
    TOK_FALSE,
    TOK_NULL,
    TOK_CINS,
    TOK_TUR,
    TOK_KUME,
    TOK_VERI,
    TOK_UYE,
    TOK_ICEAKTAR,
    TOK_DYAZ,
    TOK_GOSTER,
    TOK_UYELER,
    TOK_ICERIR,
    TOK_USTLER,
    TOK_ALTLAR,
    TOK_YOL,
    TOK_ARA,
    TOK_LISTE,
    TOK_YARDIM,
    TOK_TEMIZLE,
    TOK_CIKIS,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_COMMA,
    TOK_DOT,
    TOK_COLON,
    TOK_EQ,
    TOK_ARROW,
    TOK_AT
} GenTokenKind;

typedef struct GenToken {
    GenTokenKind kind;
    const char *lexeme;
    size_t length;
    size_t line;
    size_t column;
    size_t offset;
    int64_t int_value;
    double float_value;
} GenToken;

typedef struct {
    const char *text;
    GenTokenKind kind;
} GenKeyword;

const GenKeyword *gen_keywords(size_t *out_count);
const char *gen_token_kind_name(GenTokenKind kind);
bool gen_token_is_keyword(GenTokenKind kind);

GenResult gen_lex_all(
    GenContext *ctx,
    const char *src,
    size_t length,
    GenToken **out_tokens,
    size_t *out_count
);

void gen_tokens_free(GenToken *tokens);

#endif /* GENLANG_LEXER_H */
