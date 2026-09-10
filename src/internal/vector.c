#include "internal/vector.h"

#include "internal/common.h"
#include "memory/allocator.h"

void gen_ptrvec_init(GenPtrVec *vec)
{
    if (vec == NULL) {
        return;
    }
    vec->items = NULL;
    vec->count = 0;
    vec->capacity = 0;
}

void gen_ptrvec_free(GenPtrVec *vec)
{
    if (vec == NULL) {
        return;
    }
    gen_free(vec->items);
    vec->items = NULL;
    vec->count = 0;
    vec->capacity = 0;
}

bool gen_ptrvec_reserve(GenPtrVec *vec, size_t needed)
{
    void **grown;
    size_t cap;

    if (vec == NULL) {
        return false;
    }
    if (needed <= vec->capacity) {
        return true;
    }
    cap = vec->capacity == 0 ? 8u : vec->capacity;
    while (cap < needed) {
        if (cap > SIZE_MAX / 2u) {
            cap = needed;
            break;
        }
        cap *= 2u;
    }
    grown = (void **)gen_realloc(vec->items, cap * sizeof(void *));
    if (grown == NULL) {
        return false;
    }
    vec->items = grown;
    vec->capacity = cap;
    return true;
}

bool gen_ptrvec_push(GenPtrVec *vec, void *item)
{
    if (!gen_ptrvec_reserve(vec, vec->count + 1u)) {
        return false;
    }
    vec->items[vec->count++] = item;
    return true;
}

void gen_strvec_init(GenStrVec *vec)
{
    if (vec == NULL) {
        return;
    }
    vec->items = NULL;
    vec->count = 0;
    vec->capacity = 0;
}

void gen_strvec_free(GenStrVec *vec)
{
    if (vec == NULL) {
        return;
    }
    gen_free(vec->items);
    vec->items = NULL;
    vec->count = 0;
    vec->capacity = 0;
}

void gen_strvec_free_all(GenStrVec *vec)
{
    size_t i;

    if (vec == NULL) {
        return;
    }
    for (i = 0; i < vec->count; i++) {
        gen_free(vec->items[i]);
    }
    gen_strvec_free(vec);
}

bool gen_strvec_push(GenStrVec *vec, char *item)
{
    char **grown;
    size_t cap;

    if (vec == NULL) {
        return false;
    }
    if (vec->count + 1u > vec->capacity) {
        cap = vec->capacity == 0 ? 8u : vec->capacity * 2u;
        grown = (char **)gen_realloc(vec->items, cap * sizeof(char *));
        if (grown == NULL) {
            return false;
        }
        vec->items = grown;
        vec->capacity = cap;
    }
    vec->items[vec->count++] = item;
    return true;
}

bool gen_strvec_push_copy(GenStrVec *vec, const char *item)
{
    char *copy;

    if (item == NULL) {
        return false;
    }
    copy = gen_strdup(item);
    if (copy == NULL) {
        return false;
    }
    if (!gen_strvec_push(vec, copy)) {
        gen_free(copy);
        return false;
    }
    return true;
}

static int gen_str_cmp(const void *a, const void *b)
{
    const char *const *sa = (const char *const *)a;
    const char *const *sb = (const char *const *)b;
    return strcmp(*sa, *sb);
}

void gen_strvec_sort(GenStrVec *vec)
{
    if (vec == NULL || vec->count < 2u) {
        return;
    }
    qsort(vec->items, vec->count, sizeof(char *), gen_str_cmp);
}

bool gen_strvec_contains(const GenStrVec *vec, const char *item)
{
    size_t i;

    if (vec == NULL || item == NULL) {
        return false;
    }
    for (i = 0; i < vec->count; i++) {
        if (strcmp(vec->items[i], item) == 0) {
            return true;
        }
    }
    return false;
}
