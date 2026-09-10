#include "test.h"

void test_api(void)
{
    GenContext *ctx;
    GenDocument *doc = NULL;
    const char *name = NULL;
    const char *parent = NULL;
    GenTypeKind kind = GEN_TYPE_KIND_GENUS;
    GenLimits limits;
    char *dyaz = NULL;
    char *goster = NULL;

    TEST_ASSERT_STR(gen_version(), "0.1.0");
    TEST_ASSERT(gen_version_major() == 0);
    TEST_ASSERT(gen_version_minor() == 1);
    TEST_ASSERT(gen_version_patch() == 0);

    ctx = gen_context_create();
    TEST_ASSERT(ctx != NULL);
    TEST_ASSERT(gen_error_code(gen_context_last_error(ctx)) == GEN_OK);
    limits = gen_context_limits(ctx);
    TEST_ASSERT(limits.max_nesting_depth > 0);
    TEST_ASSERT(gen_context_set_limits(ctx, &limits) == GEN_OK);

    TEST_ASSERT(
        gen_document_parse(
            ctx,
            "cins Canli\ncins Hayvan -> Canli\nveri n = 3\n",
            &doc
        ) == GEN_OK
    );
    TEST_ASSERT(gen_type_count(doc) == 2);
    TEST_ASSERT(gen_type_name(doc, 0, &name) == GEN_OK);
    TEST_ASSERT_STR(name, "Canli");
    TEST_ASSERT(gen_type_kind(doc, 1, &kind) == GEN_OK);
    TEST_ASSERT(kind == GEN_TYPE_KIND_GENUS);
    TEST_ASSERT(gen_type_parent(doc, "Hayvan", &parent) == GEN_OK);
    TEST_ASSERT_STR(parent, "Canli");
    TEST_ASSERT(gen_entity_count(doc) == 1);
    TEST_ASSERT(gen_format_goster(doc, "n", &goster) == GEN_OK);
    TEST_ASSERT(goster != NULL);
    gen_string_free(goster);

    gen_document_free(doc);
    doc = NULL;
    TEST_ASSERT(
        gen_document_parse(
            ctx,
            "cins Kedi { ayak_sayisi = 4 }\nkume Evcil { acik = true }\n",
            &doc
        ) == GEN_OK
    );
    {
        const GenValue *props = NULL;
        const GenValue *item = NULL;
        TEST_ASSERT(gen_type_properties(doc, "Kedi", &props) == GEN_OK);
        TEST_ASSERT(props != NULL);
        TEST_ASSERT(gen_type_property(doc, "Kedi", "ayak_sayisi", &item) == GEN_OK);
        TEST_ASSERT(gen_value_int(item) == 4);
        TEST_ASSERT(gen_set_property(doc, "Evcil", "acik", &item) == GEN_OK);
        TEST_ASSERT(gen_value_bool(item) == 1);
        {
            size_t line = 0;
            size_t column = 0;
            TEST_ASSERT(gen_type_location(doc, "Kedi", &line, &column) == GEN_OK);
            TEST_ASSERT(line >= 1);
            TEST_ASSERT(gen_set_location(doc, "Evcil", &line, &column) == GEN_OK);
            TEST_ASSERT(line >= 1);
        }
    }

    gen_document_free(doc);
    doc = NULL;
    TEST_ASSERT(
        gen_document_parse(
            ctx,
            "cins T\nveri boncuk : T { isim = \"Boncuk\" }\nkume S\nuye boncuk -> S\n",
            &doc
        ) == GEN_OK
    );
        TEST_ASSERT(gen_format_dyaz(doc, "boncuk", &dyaz) == GEN_OK);
        TEST_ASSERT(strstr(dyaz, "Types:") != NULL);
        TEST_ASSERT(strstr(dyaz, "Sets:") != NULL);
        TEST_ASSERT(strstr(dyaz, "T") != NULL);
        TEST_ASSERT(strstr(dyaz, "S") != NULL);
        {
            size_t line = 0;
            size_t column = 0;
            TEST_ASSERT(gen_entity_location(doc, "boncuk", &line, &column) == GEN_OK);
            TEST_ASSERT(line >= 1);
        }
    gen_string_free(dyaz);

    gen_document_free(doc);
    gen_context_free(ctx);
    gen_document_free(NULL);
    gen_context_free(NULL);
    gen_value_free(NULL);
    gen_query_result_free(NULL);
}
