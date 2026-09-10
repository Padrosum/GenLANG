#ifndef GENLANG_TEST_H
#define GENLANG_TEST_H

#include "genlang.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int g_test_failures;
extern int g_test_count;

#define TEST_ASSERT(cond)                                                       \
    do {                                                                        \
        g_test_count++;                                                         \
        if (!(cond)) {                                                          \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
            g_test_failures++;                                                  \
        }                                                                       \
    } while (0)

#define TEST_ASSERT_EQ_INT(a, b) TEST_ASSERT((a) == (b))
#define TEST_ASSERT_STR(a, b) TEST_ASSERT((a) != NULL && (b) != NULL && strcmp((a), (b)) == 0)

void test_lexer(void);
void test_parser(void);
void test_semantic(void);
void test_runtime(void);
void test_query(void);
void test_serializer(void);
void test_api(void);
void test_integration(void);

#endif
