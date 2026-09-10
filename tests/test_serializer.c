#include "test.h"

void test_serializer(void)
{
    const char *src =
        "cins Canli\n"
        "cins Hayvan -> Canli\n"
        "kume S\n"
        "veri x : Hayvan {\n"
        "    n = 1\n"
        "}\n"
        "uye x -> S\n";
    GenContext *ctx = gen_context_create();
    GenDocument *doc = NULL;
    GenDocument *doc2 = NULL;
    char *text1 = NULL;
    char *text2 = NULL;
    size_t len1 = 0;
    size_t len2 = 0;

    TEST_ASSERT(gen_document_parse(ctx, src, &doc) == GEN_OK);
    TEST_ASSERT(gen_document_serialize(doc, &text1, &len1) == GEN_OK);
    TEST_ASSERT(text1 != NULL);
    TEST_ASSERT(len1 == strlen(text1));
    TEST_ASSERT(gen_document_serialize(doc, &text2, &len2) == GEN_OK);
    TEST_ASSERT(len1 == len2);
    TEST_ASSERT(strcmp(text1, text2) == 0);
    gen_string_free(text2);
    text2 = NULL;

    TEST_ASSERT(gen_document_parse(ctx, text1, &doc2) == GEN_OK);
    TEST_ASSERT(gen_document_serialize(doc2, &text2, &len2) == GEN_OK);
    TEST_ASSERT(strcmp(text1, text2) == 0);

    gen_string_free(text1);
    gen_string_free(text2);
    gen_document_free(doc);
    gen_document_free(doc2);

    TEST_ASSERT(gen_document_parse(ctx, src, &doc) == GEN_OK);
    TEST_ASSERT(gen_document_to_json(doc, &text1, &len1) == GEN_OK);
    TEST_ASSERT(strstr(text1, "\"kind\":\"cins\"") != NULL);
    TEST_ASSERT(strstr(text1, "\"name\":\"Hayvan\"") != NULL);
    TEST_ASSERT(strstr(text1, "\"parent\":\"Canli\"") != NULL);
    TEST_ASSERT(strstr(text1, "\"entity\":\"x\"") != NULL);
    TEST_ASSERT(strstr(text1, "\"set\":\"S\"") != NULL);
    TEST_ASSERT(gen_document_from_json(ctx, text1, &doc2) == GEN_OK);
    TEST_ASSERT(gen_entity_count(doc2) == gen_entity_count(doc));
    TEST_ASSERT(gen_type_count(doc2) == gen_type_count(doc));
    TEST_ASSERT(gen_set_count(doc2) == gen_set_count(doc));
    gen_string_free(text1);
    text1 = NULL;
    TEST_ASSERT(gen_document_to_json(doc, &text1, &len1) == GEN_OK);
    TEST_ASSERT(gen_document_to_json(doc2, &text2, &len2) == GEN_OK);
    TEST_ASSERT(strcmp(text1, text2) == 0);
    gen_string_free(text1);
    gen_string_free(text2);
    text1 = NULL;
    text2 = NULL;
    gen_document_free(doc2);
    doc2 = NULL;

    TEST_ASSERT(gen_document_to_yaml(doc, &text1, &len1) == GEN_OK);
    TEST_ASSERT(strstr(text1, "kind: cins") != NULL);
    TEST_ASSERT(strstr(text1, "name: Hayvan") != NULL);
    TEST_ASSERT(strstr(text1, "parent: Canli") != NULL);
    TEST_ASSERT(strstr(text1, "entity: x") != NULL);
    TEST_ASSERT(strstr(text1, "set: S") != NULL);
    TEST_ASSERT(gen_document_from_yaml(ctx, text1, &doc2) == GEN_OK);
    TEST_ASSERT(gen_entity_count(doc2) == gen_entity_count(doc));
    TEST_ASSERT(gen_type_count(doc2) == gen_type_count(doc));
    TEST_ASSERT(gen_set_count(doc2) == gen_set_count(doc));
    gen_string_free(text1);
    text1 = NULL;
    {
        char *yaml2 = NULL;
        TEST_ASSERT(gen_document_to_yaml(doc, &text1, &len1) == GEN_OK);
        TEST_ASSERT(gen_document_to_yaml(doc2, &yaml2, &len2) == GEN_OK);
        TEST_ASSERT(strcmp(text1, yaml2) == 0);
        gen_string_free(yaml2);
    }
    gen_string_free(text1);
    text1 = NULL;
    gen_document_free(doc2);
    doc2 = NULL;

    {
        const char *handwritten =
            "types:\n"
            "  - kind: cins\n"
            "    name: T\n"
            "    parent: null\n"
            "    properties: null\n"
            "sets: []\n"
            "entities:\n"
            "  - name: n\n"
            "    type: T\n"
            "    value:\n"
            "      items:\n"
            "        - 1\n"
            "        - \"a:b\"\n"
            "      nested:\n"
            "        k: true\n"
            "      owner:\n"
            "        $ref: n\n"
            "memberships: []\n";
        const GenValue *value = NULL;
        const GenValue *items = NULL;
        const GenValue *item = NULL;
        TEST_ASSERT(gen_document_from_yaml(ctx, handwritten, &doc2) == GEN_OK);
        TEST_ASSERT(gen_entity_value(doc2, "n", &value) == GEN_OK);
        TEST_ASSERT(gen_value_object_get(value, "items", &items) == GEN_OK);
        TEST_ASSERT(gen_value_list_count(items) == 2);
        TEST_ASSERT(gen_value_list_get(items, 1, &item) == GEN_OK);
        TEST_ASSERT_STR(gen_value_string(item), "a:b");
        TEST_ASSERT(gen_value_object_get(value, "owner", &item) == GEN_OK);
        TEST_ASSERT(gen_value_type(item) == GEN_VALUE_REFERENCE);
        TEST_ASSERT_STR(gen_value_reference(item), "n");
        gen_document_free(doc2);
        doc2 = NULL;
    }

    {
        char *json = NULL;
        TEST_ASSERT(gen_document_to_json(doc, &json, NULL) == GEN_OK);
        TEST_ASSERT(gen_document_from_yaml(ctx, json, &doc2) == GEN_OK);
        TEST_ASSERT(gen_type_count(doc2) == gen_type_count(doc));
        gen_string_free(json);
        gen_document_free(doc2);
        doc2 = NULL;
    }

    {
        char *bin = NULL;
        size_t bin_len = 0;
        GenDocument *doc3 = NULL;
        TEST_ASSERT(gen_document_to_binary(doc, &bin, &bin_len) == GEN_OK);
        TEST_ASSERT(bin != NULL);
        TEST_ASSERT(bin_len >= 4);
        TEST_ASSERT(bin[0] == 'G' && bin[1] == 'L' && bin[2] == 'B' && bin[3] == 1);
        TEST_ASSERT(gen_document_from_binary(ctx, bin, bin_len, &doc3) == GEN_OK);
        TEST_ASSERT(gen_type_count(doc3) == gen_type_count(doc));
        TEST_ASSERT(gen_entity_count(doc3) == gen_entity_count(doc));
        TEST_ASSERT(gen_set_count(doc3) == gen_set_count(doc));
        {
            char *bin2 = NULL;
            size_t bin2_len = 0;
            TEST_ASSERT(gen_document_to_binary(doc3, &bin2, &bin2_len) == GEN_OK);
            TEST_ASSERT(bin_len == bin2_len);
            TEST_ASSERT(memcmp(bin, bin2, bin_len) == 0);
            gen_string_free(bin2);
        }
        gen_string_free(bin);
        gen_document_free(doc3);
    }

    {
        const GenValue *value = NULL;
        char *json = NULL;
        TEST_ASSERT(gen_entity_value(doc, "x", &value) == GEN_OK);
        TEST_ASSERT(gen_value_to_json(value, &json, NULL) == GEN_OK);
        TEST_ASSERT(strstr(json, "\"n\":1") != NULL);
        gen_string_free(json);
        TEST_ASSERT(gen_value_to_yaml(value, &json, NULL) == GEN_OK);
        TEST_ASSERT(strstr(json, "n: 1") != NULL);
        gen_string_free(json);
    }

    gen_document_free(doc);
    gen_context_free(ctx);
}
