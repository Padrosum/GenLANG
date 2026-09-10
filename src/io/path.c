#include "io/path.h"

#include "memory/allocator.h"

#include <stdlib.h>
#include <string.h>

bool gen_path_is_absolute(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return false;
    }
#ifdef _WIN32
    if (path[0] == '\\' || path[0] == '/') {
        return true;
    }
    return ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
           path[1] == ':';
#else
    return path[0] == '/';
#endif
}

static int gen_is_sep(char c)
{
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

char *gen_path_dirname(const char *path)
{
    size_t n;
    size_t i;
    char *out;

    if (path == NULL || path[0] == '\0') {
        return gen_strdup(".");
    }
    n = strlen(path);
    while (n > 1u && gen_is_sep(path[n - 1u])) {
        n--;
    }
    i = n;
    while (i > 0u && !gen_is_sep(path[i - 1u])) {
        i--;
    }
    if (i == 0u) {
        return gen_strdup(".");
    }
    if (i == 1u) {
        return gen_strdup("/");
    }
    out = gen_strndup(path, i - 1u);
    return out;
}

char *gen_path_join(const char *dir, const char *rel)
{
    size_t dir_len;
    size_t rel_len;
    char *out;

    if (rel == NULL) {
        return NULL;
    }
    if (gen_path_is_absolute(rel) || dir == NULL || dir[0] == '\0' || strcmp(dir, ".") == 0) {
        return gen_strdup(rel);
    }
    dir_len = strlen(dir);
    rel_len = strlen(rel);
    out = (char *)gen_malloc(dir_len + rel_len + 2u);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, dir, dir_len);
    if (dir_len > 0u && !gen_is_sep(dir[dir_len - 1u])) {
        out[dir_len] = '/';
        memcpy(out + dir_len + 1u, rel, rel_len + 1u);
    } else {
        memcpy(out + dir_len, rel, rel_len + 1u);
    }
    return out;
}

char *gen_path_canonical(const char *path)
{
    char **stack = NULL;
    size_t count = 0;
    size_t cap = 0;
    const char *p;
    int abs;
    size_t i;
    size_t out_len;
    char *out;
    size_t w;

    if (path == NULL) {
        return NULL;
    }
    abs = gen_path_is_absolute(path) ? 1 : 0;
    p = path;
    if (abs && path[0] == '/') {
        p = path + 1;
    }

    while (*p != '\0') {
        const char *start;
        size_t len;
        char *comp;
        char **grown;

        while (*p != '\0' && gen_is_sep(*p)) {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        start = p;
        while (*p != '\0' && !gen_is_sep(*p)) {
            p++;
        }
        len = (size_t)(p - start);
        if (len == 1u && start[0] == '.') {
            continue;
        }
        if (len == 2u && start[0] == '.' && start[1] == '.') {
            if (count > 0u && !(strlen(stack[count - 1u]) == 2u &&
                                stack[count - 1u][0] == '.' && stack[count - 1u][1] == '.')) {
                gen_free(stack[count - 1u]);
                count--;
            } else if (!abs) {
                comp = gen_strndup("..", 2);
                if (comp == NULL) {
                    goto fail;
                }
                if (count + 1u > cap) {
                    cap = cap == 0 ? 8u : cap * 2u;
                    grown = (char **)gen_realloc(stack, cap * sizeof(char *));
                    if (grown == NULL) {
                        gen_free(comp);
                        goto fail;
                    }
                    stack = grown;
                }
                stack[count++] = comp;
            }
            continue;
        }
        comp = gen_strndup(start, len);
        if (comp == NULL) {
            goto fail;
        }
        if (count + 1u > cap) {
            cap = cap == 0 ? 8u : cap * 2u;
            grown = (char **)gen_realloc(stack, cap * sizeof(char *));
            if (grown == NULL) {
                gen_free(comp);
                goto fail;
            }
            stack = grown;
        }
        stack[count++] = comp;
    }

    if (count == 0u) {
        gen_free(stack);
        return gen_strdup(abs ? "/" : ".");
    }

    out_len = abs ? 1u : 0u;
    for (i = 0; i < count; i++) {
        out_len += strlen(stack[i]);
        if (i + 1u < count || abs) {
            out_len += 1u;
        }
    }
    if (!abs) {
        out_len += count > 0u ? (count - 1u) : 0u;
        /* separators between components already counted loosely; recompute */
        out_len = 0;
        for (i = 0; i < count; i++) {
            out_len += strlen(stack[i]);
        }
        if (count > 0u) {
            out_len += count - 1u;
        }
    } else {
        out_len = 1u;
        for (i = 0; i < count; i++) {
            out_len += strlen(stack[i]) + 1u;
        }
        if (count > 0u) {
            out_len -= 1u; /* no trailing slash except root */
        }
    }

    out = (char *)gen_malloc(out_len + 1u);
    if (out == NULL) {
        goto fail;
    }
    w = 0;
    if (abs) {
        out[w++] = '/';
    }
    for (i = 0; i < count; i++) {
        size_t clen = strlen(stack[i]);
        if (i > 0u) {
            out[w++] = '/';
        }
        memcpy(out + w, stack[i], clen);
        w += clen;
        gen_free(stack[i]);
    }
    out[w] = '\0';
    gen_free(stack);
    return out;

fail:
    for (i = 0; i < count; i++) {
        gen_free(stack[i]);
    }
    gen_free(stack);
    return NULL;
}

bool gen_path_has_gl_suffix(const char *path)
{
    size_t n;
    if (path == NULL) {
        return false;
    }
    n = strlen(path);
    return n >= 3u && path[n - 3u] == '.' && path[n - 2u] == 'g' && path[n - 1u] == 'l';
}
