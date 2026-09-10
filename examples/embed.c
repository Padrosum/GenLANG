/*
 * Query examples/packages.gl through the public C ABI.
 *
 *   cc -Iinclude examples/embed.c -Lbuild -lgenlang -o /tmp/genlang-embed
 *   LD_LIBRARY_PATH=build /tmp/genlang-embed
 */

#include <genlang.h>
#include <stdio.h>

static void print_names(const char *title, GenQueryResult *result)
{
    size_t i;
    printf("%s:\n", title);
    for (i = 0; i < gen_query_result_count(result); i++) {
        printf("  %s\n", gen_query_result_name(result, i));
    }
    gen_query_result_free(result);
}

int main(void)
{
    GenContext *ctx = gen_context_create();
    GenDocument *doc = NULL;
    GenValue *value = NULL;
    GenQueryResult *result = NULL;
    int member = 0;

    if (gen_document_load_file(ctx, "examples/packages.gl", &doc) != GEN_OK) {
        fprintf(stderr, "%s\n", gen_error_message(gen_context_last_error(ctx)));
        gen_context_free(ctx);
        return 1;
    }

    if (gen_get(doc, "pmusic.dil", &value) == GEN_OK) {
        printf("pmusic.dil = %s\n", gen_value_string(value));
        gen_value_free(value);
    }

    if (gen_types_of(doc, "pmusic", &result) == GEN_OK) {
        print_names("pmusic types", result);
    }
    if (gen_memberships_of(doc, "pmusic", &result) == GEN_OK) {
        print_names("pmusic sets", result);
    }

    if (gen_is_member(doc, "GoIle", "pmusic", &member) == GEN_OK) {
        printf("pmusic in GoIle: %s\n", member ? "true" : "false");
    }
    if (gen_set_members(doc, "CIle", &result) == GEN_OK) {
        print_names("CIle", result);
    }

    gen_document_free(doc);
    gen_context_free(ctx);
    return 0;
}
