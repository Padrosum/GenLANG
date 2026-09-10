#include "test.h"

int g_test_failures = 0;
int g_test_count = 0;

int main(void)
{
    test_lexer();
    test_parser();
    test_semantic();
    test_runtime();
    test_query();
    test_serializer();
    test_api();
    test_integration();

    if (g_test_failures != 0) {
        fprintf(stderr, "%d/%d tests failed\n", g_test_failures, g_test_count);
        return 1;
    }
    printf("OK %d tests\n", g_test_count);
    return 0;
}
