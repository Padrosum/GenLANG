#include "test.h"

void test_integration(void)
{
    GenContext *ctx = gen_context_create();
    GenDocument *doc = NULL;
    GenValue *value = NULL;
    GenQueryResult *result = NULL;
    char *serialized = NULL;
    int is_member = 0;

    TEST_ASSERT(gen_document_load_file(ctx, "examples/animals.gl", &doc) == GEN_OK);
    TEST_ASSERT(gen_format_dyaz(doc, "boncuk", &serialized) == GEN_OK);
    TEST_ASSERT(strstr(serialized, "Kedi") != NULL);
    TEST_ASSERT(strstr(serialized, "EvcilHayvanlar") != NULL);
    TEST_ASSERT(strstr(serialized, "SiyahHayvanlar") != NULL);
    gen_string_free(serialized);
    serialized = NULL;

    TEST_ASSERT(gen_ancestors_of(doc, "Kedi", &result) == GEN_OK);
    TEST_ASSERT(gen_query_result_count(result) == 3);
    gen_query_result_free(result);
    TEST_ASSERT(gen_set_members(doc, "EvcilHayvanlar", &result) == GEN_OK);
    TEST_ASSERT(gen_query_result_count(result) == 2);
    gen_query_result_free(result);
    TEST_ASSERT(gen_is_member(doc, "SiyahHayvanlar", "boncuk", &is_member) == GEN_OK);
    TEST_ASSERT(is_member == 1);
    TEST_ASSERT(gen_document_serialize(doc, &serialized, NULL) == GEN_OK);
    TEST_ASSERT(strstr(serialized, "cins Canli") != NULL);
    gen_string_free(serialized);
    TEST_ASSERT(gen_document_to_json(doc, &serialized, NULL) == GEN_OK);
    TEST_ASSERT(strstr(serialized, "\"name\":\"boncuk\"") != NULL);
    TEST_ASSERT(strstr(serialized, "\"set\":\"EvcilHayvanlar\"") != NULL);
    gen_string_free(serialized);
    TEST_ASSERT(gen_document_to_yaml(doc, &serialized, NULL) == GEN_OK);
    TEST_ASSERT(strstr(serialized, "name: boncuk") != NULL);
    TEST_ASSERT(strstr(serialized, "set: EvcilHayvanlar") != NULL);
    gen_string_free(serialized);
    {
        char *bin = NULL;
        size_t bin_len = 0;
        GenDocument *again = NULL;
        TEST_ASSERT(gen_document_to_binary(doc, &bin, &bin_len) == GEN_OK);
        TEST_ASSERT(gen_document_from_binary(ctx, bin, bin_len, &again) == GEN_OK);
        TEST_ASSERT(gen_entity_count(again) == gen_entity_count(doc));
        gen_string_free(bin);
        gen_document_free(again);
    }
    gen_document_free(doc);

    TEST_ASSERT(gen_document_load_file(ctx, "examples/nested.gl", &doc) == GEN_OK);
    TEST_ASSERT(gen_get(doc, "x.a[1].b[2]", &value) == GEN_OK);
    TEST_ASSERT(gen_value_int(value) == 60);
    gen_value_free(value);
    gen_document_free(doc);

    TEST_ASSERT(gen_document_load_file(ctx, "examples/relations.gl", &doc) == GEN_OK);
    TEST_ASSERT(gen_entity_count(doc) >= 1);
    gen_document_free(doc);

    TEST_ASSERT(gen_document_load_file(ctx, "examples/import/main.gl", &doc) == GEN_OK);
    TEST_ASSERT(gen_type_count(doc) == 4);
    TEST_ASSERT(gen_entity_count(doc) == 1);
    gen_document_free(doc);
    doc = NULL;

    TEST_ASSERT(gen_document_load_file(ctx, "examples/schema.gl", &doc) == GEN_OK);
    TEST_ASSERT(gen_get(doc, "boncuk.ekstra", &value) == GEN_OK);
    TEST_ASSERT(gen_value_bool(value) == 1);
    gen_value_free(value);
    gen_document_free(doc);

    TEST_ASSERT(gen_document_load_file(ctx, "examples/unicode.gl", &doc) == GEN_OK);
    TEST_ASSERT(gen_is_member(doc, "Siyahlar", "böncü", &is_member) == GEN_OK);
    TEST_ASSERT(is_member == 1);
    gen_document_free(doc);

    TEST_ASSERT(gen_document_load_file(ctx, "examples/music.gl", &doc) == GEN_OK);
    TEST_ASSERT(gen_get(doc, "parca_01.sure_sn", &value) == GEN_OK);
    TEST_ASSERT(gen_value_int(value) == 214);
    gen_value_free(value);
    TEST_ASSERT(gen_is_member(doc, "Gece", "parca_02", &is_member) == GEN_OK);
    TEST_ASSERT(is_member == 1);
    gen_document_free(doc);

    {
        const char *type_name = NULL;
        TEST_ASSERT(gen_document_load_file(ctx, "examples/packages.gl", &doc) == GEN_OK);
        TEST_ASSERT(gen_entity_type_name(doc, "pmusic", &type_name) == GEN_OK);
        TEST_ASSERT_STR(type_name, "TerminalAraci");
        TEST_ASSERT(gen_is_member(doc, "CIle", "pnot", &is_member) == GEN_OK);
        TEST_ASSERT(is_member == 1);
        gen_document_free(doc);
    }

    TEST_ASSERT(gen_document_load_file(ctx, "examples/notes.gl", &doc) == GEN_OK);
    TEST_ASSERT(gen_get(doc, "luma_bellek.proje", &value) == GEN_OK);
    TEST_ASSERT(gen_value_type(value) == GEN_VALUE_REFERENCE);
    TEST_ASSERT_STR(gen_value_reference(value), "luma_os");
    gen_value_free(value);
    gen_document_free(doc);
    doc = NULL;

    TEST_ASSERT(gen_document_load_file(ctx, "tests/fixtures/import/diamond.gl", &doc) == GEN_OK);
    TEST_ASSERT(gen_type_count(doc) == 4);
    gen_document_free(doc);
    doc = NULL;

    TEST_ASSERT(gen_document_load_file(ctx, "tests/fixtures/import/twice.gl", &doc) == GEN_OK);
    TEST_ASSERT(gen_type_count(doc) == 2);
    gen_document_free(doc);
    doc = NULL;

    TEST_ASSERT(
        gen_document_load_file(ctx, "tests/fixtures/import/cycle_a.gl", &doc) == GEN_ERR_CYCLE
    );
    TEST_ASSERT(doc == NULL);
    TEST_ASSERT(gen_error_code(gen_context_last_error(ctx)) == GEN_ERR_CYCLE);
    gen_document_free(doc);

    {
        const char *src = "iceaktar \"types.gl\"\ncins Extra\n";
        TEST_ASSERT(gen_document_parse_at(ctx, src, "examples/import/main.gl", &doc) == GEN_OK);
        TEST_ASSERT(gen_type_count(doc) == 4);
        gen_document_free(doc);
        doc = NULL;
        TEST_ASSERT(gen_error_path(gen_context_last_error(ctx)) == NULL ||
                    gen_error_code(gen_context_last_error(ctx)) == GEN_OK);
    }

    gen_context_free(ctx);
}
