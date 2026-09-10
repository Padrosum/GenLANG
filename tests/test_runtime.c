#include "test.h"

void test_runtime(void)
{
    GenContext *ctx = gen_context_create();
    GenDocument *doc = NULL;
    const GenValue *value = NULL;
    const GenValue *item = NULL;
    GenValue *owned = NULL;
    const char *key = NULL;

    TEST_ASSERT(
        gen_document_parse(
            ctx,
            "veri numbers = [10, 20, 30, 40]\n"
            "veri person {\n  name = \"Ahmet\"\n  age = 25\n}\n"
            "veri nested {\n  a = [{ b = [10, 20, 30] }, { b = [40, 50, 60] }]\n}\n"
            "cins T\n"
            "veri owner = @person\n",
            &doc
        ) == GEN_OK
    );

    TEST_ASSERT(gen_entity_value(doc, "numbers", &value) == GEN_OK);
    TEST_ASSERT(gen_value_type(value) == GEN_VALUE_LIST);
    TEST_ASSERT(gen_value_list_count(value) == 4);
    TEST_ASSERT(gen_value_list_get(value, 0, &item) == GEN_OK);
    TEST_ASSERT(gen_value_int(item) == 10);
    TEST_ASSERT(gen_value_list_get(value, 3, &item) == GEN_OK);
    TEST_ASSERT(gen_value_int(item) == 40);
    TEST_ASSERT(gen_value_list_get(value, 4, &item) == GEN_ERR_INDEX);

    TEST_ASSERT(gen_entity_value(doc, "person", &value) == GEN_OK);
    TEST_ASSERT(gen_value_type(value) == GEN_VALUE_OBJECT);
    TEST_ASSERT(gen_value_object_get(value, "name", &item) == GEN_OK);
    TEST_ASSERT_STR(gen_value_string(item), "Ahmet");
    TEST_ASSERT(gen_value_object_key(value, 0, &key) == GEN_OK);

    TEST_ASSERT(gen_get(doc, "nested.a[1].b[2]", &owned) == GEN_OK);
    TEST_ASSERT(gen_value_type(owned) == GEN_VALUE_INT);
    TEST_ASSERT(gen_value_int(owned) == 60);
    gen_value_free(owned);

    TEST_ASSERT(gen_entity_value(doc, "owner", &value) == GEN_OK);
    TEST_ASSERT(gen_value_type(value) == GEN_VALUE_REFERENCE);
    TEST_ASSERT_STR(gen_value_reference(value), "person");

    TEST_ASSERT(gen_value_clone(value, &owned) == GEN_OK);
    TEST_ASSERT(gen_value_type(owned) == GEN_VALUE_REFERENCE);
    gen_value_free(owned);

    gen_document_free(doc);
    gen_context_free(ctx);
}
