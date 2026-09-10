#ifndef GENLANG_PATH_H
#define GENLANG_PATH_H

#include <stdbool.h>
#include <stddef.h>

bool gen_path_is_absolute(const char *path);
char *gen_path_dirname(const char *path);
char *gen_path_join(const char *dir, const char *rel);
char *gen_path_canonical(const char *path);
bool gen_path_has_gl_suffix(const char *path);

#endif /* GENLANG_PATH_H */
