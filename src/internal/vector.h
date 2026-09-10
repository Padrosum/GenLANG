#ifndef GENLANG_INTERNAL_VECTOR_H
#define GENLANG_INTERNAL_VECTOR_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    void **items;
    size_t count;
    size_t capacity;
} GenPtrVec;

void gen_ptrvec_init(GenPtrVec *vec);
void gen_ptrvec_free(GenPtrVec *vec);
bool gen_ptrvec_reserve(GenPtrVec *vec, size_t needed);
bool gen_ptrvec_push(GenPtrVec *vec, void *item);

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} GenStrVec;

void gen_strvec_init(GenStrVec *vec);
void gen_strvec_free(GenStrVec *vec);
void gen_strvec_free_all(GenStrVec *vec);
bool gen_strvec_push(GenStrVec *vec, char *item);
bool gen_strvec_push_copy(GenStrVec *vec, const char *item);
void gen_strvec_sort(GenStrVec *vec);
bool gen_strvec_contains(const GenStrVec *vec, const char *item);

#endif /* GENLANG_INTERNAL_VECTOR_H */
