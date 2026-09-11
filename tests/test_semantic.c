#include "test.h"

static GenResult parse_rc(const char *src)
{
    GenContext *ctx = gen_context_create();
    GenDocument *doc = NULL;
    GenResult rc = gen_document_parse(ctx, src, &doc);
    gen_document_free(doc);
    gen_context_free(ctx);
    return rc;
}

void test_semantic(void)
{
    TEST_ASSERT(parse_rc("cins A\ncins A\n") == GEN_ERR_DUPLICATE);
    TEST_ASSERT(parse_rc("veri a = 1\nveri a = 2\n") == GEN_ERR_DUPLICATE);
    TEST_ASSERT(parse_rc("kume S\nkume S\n") == GEN_ERR_DUPLICATE);
    TEST_ASSERT(parse_rc("cins A -> Missing\n") == GEN_ERR_SEMANTIC);
    TEST_ASSERT(parse_rc("veri x : Missing { a = 1 }\n") == GEN_ERR_SEMANTIC);
    TEST_ASSERT(parse_rc("uye x -> S\nkume S\n") == GEN_ERR_SEMANTIC);
    TEST_ASSERT(parse_rc("veri x = 1\nuye x -> Missing\n") == GEN_ERR_SEMANTIC);
    TEST_ASSERT(parse_rc("cins A -> A\n") == GEN_ERR_CYCLE);
    TEST_ASSERT(parse_rc("cins A -> B\ncins B -> C\ncins C -> A\n") == GEN_ERR_CYCLE);
    TEST_ASSERT(parse_rc("veri x = @missing\n") == GEN_ERR_SEMANTIC);
    TEST_ASSERT(parse_rc("cins T\nveri a : T { n = 1 }\nkume S\nuye a -> S\n") == GEN_OK);
    TEST_ASSERT(parse_rc("cins T { n = 1 }\nveri a : T { n = 2 }\n") == GEN_OK);
    TEST_ASSERT(parse_rc("cins T { n = 1 }\nveri a : T { n = 2, extra = true }\n") == GEN_OK);
    TEST_ASSERT(parse_rc("cins T { n = 1 }\nveri a : T { }\n") == GEN_ERR_SEMANTIC);
    TEST_ASSERT(parse_rc("cins T { n = 1 }\nveri a : T { n = \"x\" }\n") == GEN_ERR_SEMANTIC);
    TEST_ASSERT(parse_rc("cins T { n = 1 }\nveri a : T = 1\n") == GEN_ERR_SEMANTIC);
    TEST_ASSERT(parse_rc("cins U { n = 1 }\ncins T -> U { s = \"\" }\nveri a : T { n = 1, s = \"ok\" }\n") == GEN_OK);
    TEST_ASSERT(parse_rc("cins U { n = 1 }\ncins T -> U { s = \"\" }\nveri a : T { s = \"ok\" }\n") == GEN_ERR_SEMANTIC);

    TEST_ASSERT(parse_rc(
        "cins Kisi\n"
        "cins Kayit { sahibi = @Kisi }\n"
        "veri ahmet : Kisi { n = 1 }\n"
        "veri r : Kayit { sahibi = @ahmet }\n"
    ) == GEN_OK);
    TEST_ASSERT(parse_rc(
        "cins Kisi\n"
        "cins Hayvan\n"
        "cins Kayit { sahibi = @Kisi }\n"
        "veri kedi : Hayvan { n = 1 }\n"
        "veri r : Kayit { sahibi = @kedi }\n"
    ) == GEN_ERR_SEMANTIC);

    {
        GenContext *ctx = gen_context_create();
        GenDocument *doc = NULL;
        const GenError *err;
        TEST_ASSERT(gen_document_parse(ctx, "veri x = @missing\n", &doc) != GEN_OK);
        err = gen_context_last_error(ctx);
        TEST_ASSERT(gen_error_line(err) >= 1);
        TEST_ASSERT(gen_error_column(err) >= 1);
        gen_document_free(doc);
        gen_context_free(ctx);
    }

    {
        GenContext *ctx = gen_context_create();
        GenDocument *doc = NULL;
        TEST_ASSERT(
            gen_document_parse(ctx, "cins A\ncins A\nkume S\nkume S\n", &doc) != GEN_OK
        );
        TEST_ASSERT(gen_context_error_count(ctx) >= 2);
        gen_document_free(doc);
        gen_context_free(ctx);
    }
}
