#ifndef GENLANG_CLI_COMMANDS_H
#define GENLANG_CLI_COMMANDS_H

#include "genlang.h"

#include <stdio.h>

#define GEN_EXIT_OK 0
#define GEN_EXIT_USAGE 1
#define GEN_EXIT_PARSE 2
#define GEN_EXIT_SEMANTIC 3
#define GEN_EXIT_IO 4

int gen_exit_from_result(GenResult result);
void gen_cli_print_error(const char *path, const char *source, const GenContext *ctx, FILE *err);
void gen_cli_print_help(FILE *out);
int gen_cli_print_document(const char *path, const GenDocument *doc, FILE *out);
int gen_cli_exec_line(GenDocument *doc, const char *line, FILE *out, FILE *err, int *out_exit_repl);

#endif /* GENLANG_CLI_COMMANDS_H */
