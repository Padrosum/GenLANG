#ifndef GENLANG_ALLOCATOR_H
#define GENLANG_ALLOCATOR_H

#include <stddef.h>

void *gen_malloc(size_t n);
void *gen_calloc(size_t count, size_t size);
void *gen_realloc(void *ptr, size_t n);
void gen_free(void *ptr);

char *gen_strdup(const char *s);
char *gen_strndup(const char *s, size_t n);

typedef struct GenArena GenArena;

GenArena *gen_arena_create(void);
void *gen_arena_alloc(GenArena *arena, size_t n);
char *gen_arena_strdup(GenArena *arena, const char *s);
char *gen_arena_strndup(GenArena *arena, const char *s, size_t n);
void gen_arena_destroy(GenArena *arena);

#endif /* GENLANG_ALLOCATOR_H */
