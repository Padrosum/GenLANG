#include "memory/allocator.h"

#include "internal/common.h"

void *gen_malloc(size_t n)
{
    if (n == 0) {
        n = 1;
    }
    return malloc(n);
}

void *gen_calloc(size_t count, size_t size)
{
    if (count == 0 || size == 0) {
        return calloc(1, 1);
    }
    if (size != 0 && count > SIZE_MAX / size) {
        return NULL;
    }
    return calloc(count, size);
}

void *gen_realloc(void *ptr, size_t n)
{
    if (n == 0) {
        n = 1;
    }
    return realloc(ptr, n);
}

void gen_free(void *ptr)
{
    free(ptr);
}

char *gen_strdup(const char *s)
{
    size_t n;

    if (s == NULL) {
        return NULL;
    }
    n = strlen(s);
    return gen_strndup(s, n);
}

char *gen_strndup(const char *s, size_t n)
{
    char *out;

    if (s == NULL) {
        return NULL;
    }
    out = (char *)gen_malloc(n + 1u);
    if (out == NULL) {
        return NULL;
    }
    if (n > 0) {
        memcpy(out, s, n);
    }
    out[n] = '\0';
    return out;
}

typedef struct GenArenaBlock {
    struct GenArenaBlock *next;
    size_t used;
    size_t cap;
    unsigned char data[];
} GenArenaBlock;

struct GenArena {
    GenArenaBlock *head;
};

static GenArenaBlock *gen_arena_new_block(size_t cap)
{
    GenArenaBlock *block;

    if (cap < 4096u) {
        cap = 4096u;
    }
    block = (GenArenaBlock *)gen_malloc(sizeof(GenArenaBlock) + cap);
    if (block == NULL) {
        return NULL;
    }
    block->next = NULL;
    block->used = 0;
    block->cap = cap;
    return block;
}

GenArena *gen_arena_create(void)
{
    GenArena *arena = (GenArena *)gen_calloc(1, sizeof(GenArena));
    if (arena == NULL) {
        return NULL;
    }
    arena->head = gen_arena_new_block(4096u);
    if (arena->head == NULL) {
        gen_free(arena);
        return NULL;
    }
    return arena;
}

void *gen_arena_alloc(GenArena *arena, size_t n)
{
    GenArenaBlock *block;
    size_t aligned;
    void *p;

    if (arena == NULL || n == 0) {
        return NULL;
    }
    aligned = gen_align8(n);
    block = arena->head;
    if (block == NULL || block->used + aligned > block->cap) {
        size_t cap = aligned;
        if (block != NULL && block->cap * 2u > cap) {
            cap = block->cap * 2u;
        }
        block = gen_arena_new_block(cap);
        if (block == NULL) {
            return NULL;
        }
        block->next = arena->head;
        arena->head = block;
    }
    p = block->data + block->used;
    block->used += aligned;
    return p;
}

char *gen_arena_strdup(GenArena *arena, const char *s)
{
    if (s == NULL) {
        return NULL;
    }
    return gen_arena_strndup(arena, s, strlen(s));
}

char *gen_arena_strndup(GenArena *arena, const char *s, size_t n)
{
    char *out = (char *)gen_arena_alloc(arena, n + 1u);
    if (out == NULL) {
        return NULL;
    }
    if (n > 0 && s != NULL) {
        memcpy(out, s, n);
    }
    out[n] = '\0';
    return out;
}

void gen_arena_destroy(GenArena *arena)
{
    GenArenaBlock *block;

    if (arena == NULL) {
        return;
    }
    block = arena->head;
    while (block != NULL) {
        GenArenaBlock *next = block->next;
        gen_free(block);
        block = next;
    }
    gen_free(arena);
}
