#include "test.h"

static const char *ANIMALS =
    "cins Canli\n"
    "cins Hayvan -> Canli\n"
    "cins Memeli -> Hayvan\n"
    "tur Kedi -> Memeli\n"
    "tur Kopek -> Memeli\n"
    "kume EvcilHayvanlar\n"
    "kume SiyahHayvanlar\n"
    "veri boncuk : Kedi {\n"
    "    isim = \"Boncuk\"\n"
    "    yas = 4\n"
    "    renk = \"siyah\"\n"
    "}\n"
    "veri karamel : Kopek {\n"
    "    isim = \"Karamel\"\n"
    "    yas = 7\n"
    "    renk = \"kahverengi\"\n"
    "}\n"
    "uye boncuk -> EvcilHayvanlar\n"
    "uye boncuk -> SiyahHayvanlar\n"
    "uye karamel -> EvcilHayvanlar\n";

void test_query(void)
{
    GenContext *ctx = gen_context_create();
    GenDocument *doc = NULL;
    GenQueryResult *result = NULL;
    GenValue *value = NULL;
    int is_member = 0;

    TEST_ASSERT(gen_document_parse(ctx, ANIMALS, &doc) == GEN_OK);

    TEST_ASSERT(gen_types_of(doc, "boncuk", &result) == GEN_OK);
    TEST_ASSERT(gen_query_result_count(result) == 4);
    TEST_ASSERT_STR(gen_query_result_name(result, 0), "Kedi");
    TEST_ASSERT_STR(gen_query_result_name(result, 1), "Memeli");
    TEST_ASSERT_STR(gen_query_result_name(result, 2), "Hayvan");
    TEST_ASSERT_STR(gen_query_result_name(result, 3), "Canli");
    gen_query_result_free(result);

    TEST_ASSERT(gen_ancestors_of(doc, "Kedi", &result) == GEN_OK);
    TEST_ASSERT(gen_query_result_count(result) == 3);
    TEST_ASSERT_STR(gen_query_result_name(result, 0), "Memeli");
    TEST_ASSERT_STR(gen_query_result_name(result, 1), "Hayvan");
    TEST_ASSERT_STR(gen_query_result_name(result, 2), "Canli");
    gen_query_result_free(result);

    TEST_ASSERT(gen_descendants_of(doc, "Hayvan", &result) == GEN_OK);
    TEST_ASSERT(gen_query_result_count(result) >= 2);
    gen_query_result_free(result);

    TEST_ASSERT(gen_memberships_of(doc, "boncuk", &result) == GEN_OK);
    TEST_ASSERT(gen_query_result_count(result) == 2);
    TEST_ASSERT_STR(gen_query_result_name(result, 0), "EvcilHayvanlar");
    TEST_ASSERT_STR(gen_query_result_name(result, 1), "SiyahHayvanlar");
    gen_query_result_free(result);

    TEST_ASSERT(gen_set_members(doc, "EvcilHayvanlar", &result) == GEN_OK);
    TEST_ASSERT(gen_query_result_count(result) == 2);
    gen_query_result_free(result);

    TEST_ASSERT(gen_is_member(doc, "SiyahHayvanlar", "boncuk", &is_member) == GEN_OK);
    TEST_ASSERT(is_member == 1);
    TEST_ASSERT(gen_is_member(doc, "SiyahHayvanlar", "karamel", &is_member) == GEN_OK);
    TEST_ASSERT(is_member == 0);

    TEST_ASSERT(gen_type_path(doc, "Kedi", "Canli", &result) == GEN_OK);
    TEST_ASSERT(gen_query_result_count(result) == 4);
    gen_query_result_free(result);

    TEST_ASSERT(gen_search(doc, "siyah", &result) == GEN_OK);
    TEST_ASSERT(gen_query_result_count(result) >= 1);
    gen_query_result_free(result);

    TEST_ASSERT(gen_eval_path(doc, "boncuk.yas", &value) == GEN_OK);
    TEST_ASSERT(gen_value_int(value) == 4);
    gen_value_free(value);

    TEST_ASSERT(gen_entities_of(doc, "Kedi", &result) == GEN_OK);
    TEST_ASSERT(gen_query_result_count(result) == 1);
    TEST_ASSERT_STR(gen_query_result_name(result, 0), "boncuk");
    gen_query_result_free(result);

    TEST_ASSERT(gen_entities_of(doc, "Memeli", &result) == GEN_OK);
    TEST_ASSERT(gen_query_result_count(result) == 2);
    gen_query_result_free(result);

    TEST_ASSERT(gen_entities_of(doc, "Missing", &result) == GEN_ERR_NOT_FOUND);

    gen_document_free(doc);
    gen_context_free(ctx);
}
