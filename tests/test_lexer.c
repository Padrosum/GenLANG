#include "test.h"

#include "error/error.h"
#include "lexer/lexer.h"

static void test_lex_ok(const char *src, GenTokenKind first)
{
    GenContext *ctx = gen_context_create();
    GenToken *tokens = NULL;
    size_t count = 0;
    GenResult rc = gen_lex_all(ctx, src, strlen(src), &tokens, &count);
    TEST_ASSERT(rc == GEN_OK);
    TEST_ASSERT(count >= 2);
    TEST_ASSERT(tokens[0].kind == first);
    TEST_ASSERT(tokens[count - 1u].kind == TOK_EOF);
    gen_tokens_free(tokens);
    gen_context_free(ctx);
}

void test_lexer(void)
{
    GenContext *ctx;
    GenToken *tokens = NULL;
    size_t count = 0;
    size_t nkw = 0;
    const GenKeyword *kws;

    kws = gen_keywords(&nkw);
    TEST_ASSERT(kws != NULL);
    TEST_ASSERT(nkw >= 20);

    test_lex_ok("cins", TOK_CINS);
    test_lex_ok("tur", TOK_TUR);
    test_lex_ok("kume", TOK_KUME);
    test_lex_ok("veri", TOK_VERI);
    test_lex_ok("uye", TOK_UYE);
    test_lex_ok("iceaktar", TOK_ICEAKTAR);
    test_lex_ok("Boncuk_1", TOK_IDENT);
    test_lex_ok("Canlı", TOK_IDENT);
    test_lex_ok("\"hello\"", TOK_STRING);
    test_lex_ok("42", TOK_INT);
    test_lex_ok("-7", TOK_INT);
    test_lex_ok("3.14", TOK_FLOAT);
    test_lex_ok("1e10", TOK_FLOAT);
    test_lex_ok("true", TOK_TRUE);
    test_lex_ok("false", TOK_FALSE);
    test_lex_ok("null", TOK_NULL);
    test_lex_ok("->", TOK_ARROW);
    test_lex_ok("{", TOK_LBRACE);
    test_lex_ok("}", TOK_RBRACE);
    test_lex_ok("[", TOK_LBRACKET);
    test_lex_ok("]", TOK_RBRACKET);
    test_lex_ok(".", TOK_DOT);
    test_lex_ok(",", TOK_COMMA);
    test_lex_ok("=", TOK_EQ);
    test_lex_ok("@", TOK_AT);
    test_lex_ok("# comment\ncins", TOK_CINS);

    ctx = gen_context_create();
    TEST_ASSERT(gen_lex_all(ctx, "\"abc\"", 5, &tokens, &count) == GEN_OK);
    TEST_ASSERT(tokens[0].kind == TOK_STRING);
    gen_tokens_free(tokens);
    tokens = NULL;
    TEST_ASSERT(gen_lex_all(ctx, "$", 1, &tokens, &count) == GEN_ERR_LEX);
    TEST_ASSERT(gen_error_code(gen_context_last_error(ctx)) == GEN_ERR_LEX);
    gen_tokens_free(tokens);
    gen_context_free(ctx);
}
