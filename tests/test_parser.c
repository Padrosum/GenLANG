#include "test.h"

static GenDocument *parse_ok(const char *src)
{
    GenContext *ctx = gen_context_create();
    GenDocument *doc = NULL;
    GenResult rc = gen_document_parse(ctx, src, &doc);
    TEST_ASSERT(rc == GEN_OK);
    TEST_ASSERT(doc != NULL);
    gen_context_free(ctx);
    return doc;
}

static void parse_fail(const char *src, GenResult expected)
{
    GenContext *ctx = gen_context_create();
    GenDocument *doc = NULL;
    GenResult rc = gen_document_parse(ctx, src, &doc);
    TEST_ASSERT(rc == expected);
    TEST_ASSERT(doc == NULL);
    gen_document_free(doc);
    gen_context_free(ctx);
}

void test_parser(void)
{
    GenDocument *doc;

    doc = parse_ok("cins Canlı\n");
    TEST_ASSERT(gen_type_count(doc) == 1);
    gen_document_free(doc);

    doc = parse_ok("cins Hayvan\nveri böncü : Hayvan { isim = \"Boncuk\" }\n");
    TEST_ASSERT(gen_entity_count(doc) == 1);
    gen_document_free(doc);

    doc = parse_ok("cins Hayvan -> Canli\ncins Canli\n");
    TEST_ASSERT(gen_type_count(doc) == 2);
    gen_document_free(doc);

    doc = parse_ok("tur Kedi -> Memeli\ncins Memeli\n");
    TEST_ASSERT(gen_type_count(doc) == 2);
    gen_document_free(doc);

    doc = parse_ok("kume EvcilHayvanlar\n");
    TEST_ASSERT(gen_set_count(doc) == 1);
    gen_document_free(doc);

    doc = parse_ok("veri numbers = [10, 20, 30, 40]\n");
    TEST_ASSERT(gen_entity_count(doc) == 1);
    gen_document_free(doc);

    doc = parse_ok("veri person {\n  name = \"Ahmet\"\n  age = 25\n}\n");
    TEST_ASSERT(gen_entity_count(doc) == 1);
    gen_document_free(doc);

    doc = parse_ok(
        "cins T\nveri a : T { x = 1 }\nkume S\nuye a -> S\n"
    );
    TEST_ASSERT(gen_entity_count(doc) == 1);
    TEST_ASSERT(gen_set_count(doc) == 1);
    gen_document_free(doc);

    doc = parse_ok(
        "veri x {\n  a = [\n    { b = [10, 20, 30] },\n    { b = [40, 50, 60] }\n  ]\n}\n"
    );
    TEST_ASSERT(gen_entity_count(doc) == 1);
    gen_document_free(doc);

    parse_fail("veri x = {\n", GEN_ERR_PARSE);
    parse_fail("cins\n", GEN_ERR_PARSE);
    parse_fail("veri x = [1, 2\n", GEN_ERR_PARSE);
    parse_fail("@@@\n", GEN_ERR_PARSE);
    parse_fail("iceaktar \"missing.gl\"\n", GEN_ERR_IO);
    parse_fail("iceaktar \"nope.txt\"\n", GEN_ERR_SEMANTIC);
    parse_fail("iceaktar\n", GEN_ERR_PARSE);
}
