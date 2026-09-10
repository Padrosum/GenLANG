#include "commands.h"
#include "repl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *gen_read_file(const char *path, int *io_error, size_t *out_length)
{
    FILE *fp;
    char *buf;
    size_t cap = 4096;
    size_t used = 0;
    size_t nread;

    *io_error = 0;
    if (out_length != NULL) {
        *out_length = 0;
    }
    fp = fopen(path, "rb");
    if (fp == NULL) {
        *io_error = 1;
        return NULL;
    }
    buf = (char *)malloc(cap);
    if (buf == NULL) {
        fclose(fp);
        *io_error = 1;
        return NULL;
    }
    for (;;) {
        if (used + 1u >= cap) {
            char *grown;
            size_t next = cap * 2u;
            grown = (char *)realloc(buf, next);
            if (grown == NULL) {
                free(buf);
                fclose(fp);
                *io_error = 1;
                return NULL;
            }
            buf = grown;
            cap = next;
        }
        nread = fread(buf + used, 1, cap - used - 1u, fp);
        used += nread;
        if (nread == 0) {
            break;
        }
    }
    fclose(fp);
    buf[used] = '\0';
    if (out_length != NULL) {
        *out_length = used;
    }
    return buf;
}

static int gen_run_file(const char *path, int check_only)
{
    GenContext *ctx;
    GenDocument *doc = NULL;
    GenResult rc;
    char *source;
    int io_error = 0;
    int exit_code;

    source = gen_read_file(path, &io_error, NULL);
    ctx = gen_context_create();
    if (ctx == NULL) {
        free(source);
        fputs("error: out of memory\n", stderr);
        return GEN_EXIT_USAGE;
    }
    if (source == NULL) {
        fprintf(stderr, "%s: I/O error: cannot read file\n", path);
        gen_context_free(ctx);
        return GEN_EXIT_IO;
    }
    rc = gen_document_parse_at(ctx, source, path, &doc);
    if (rc != GEN_OK) {
        gen_cli_print_error(path, source, ctx, stderr);
        exit_code = gen_exit_from_result(rc);
        free(source);
        gen_context_free(ctx);
        return exit_code;
    }
    if (check_only) {
        fprintf(stdout, "OK %s\n", path);
        exit_code = GEN_EXIT_OK;
    } else {
        exit_code = gen_cli_print_document(path, doc, stdout);
    }
    gen_document_free(doc);
    gen_context_free(ctx);
    free(source);
    return exit_code;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        gen_cli_print_help(stderr);
        return GEN_EXIT_USAGE;
    }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        gen_cli_print_help(stdout);
        return GEN_EXIT_OK;
    }
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        printf("genlang %s\n", gen_version());
        return GEN_EXIT_OK;
    }
    if (strcmp(argv[1], "check") == 0) {
        if (argc != 3) {
            fputs("usage: genlang check <file.gl>\n", stderr);
            return GEN_EXIT_USAGE;
        }
        return gen_run_file(argv[2], 1);
    }
    if (strcmp(argv[1], "repl") == 0) {
        return gen_cli_repl(argc >= 3 ? argv[2] : NULL);
    }
    if (strcmp(argv[1], "query") == 0) {
        GenContext *ctx;
        GenDocument *doc = NULL;
        GenValue *value = NULL;
        char *text = NULL;
        char *source;
        int io_error = 0;
        GenResult rc;
        int exit_code;

        if (argc != 4) {
            fputs("usage: genlang query <file.gl> <path>\n", stderr);
            return GEN_EXIT_USAGE;
        }
        source = gen_read_file(argv[2], &io_error, NULL);
        ctx = gen_context_create();
        if (ctx == NULL) {
            free(source);
            fputs("error: out of memory\n", stderr);
            return GEN_EXIT_USAGE;
        }
        if (source == NULL) {
            fprintf(stderr, "%s: I/O error: cannot read file\n", argv[2]);
            gen_context_free(ctx);
            return GEN_EXIT_IO;
        }
        rc = gen_document_parse_at(ctx, source, argv[2], &doc);
        if (rc != GEN_OK) {
            gen_cli_print_error(argv[2], source, ctx, stderr);
            exit_code = gen_exit_from_result(rc);
            free(source);
            gen_context_free(ctx);
            return exit_code;
        }
        rc = gen_eval_path(doc, argv[3], &value);
        if (rc != GEN_OK) {
            fprintf(stderr, "error: cannot evaluate path '%s'\n", argv[3]);
            gen_document_free(doc);
            gen_context_free(ctx);
            free(source);
            return GEN_EXIT_USAGE;
        }
        rc = gen_value_format(value, &text, NULL);
        gen_value_free(value);
        if (rc != GEN_OK) {
            fprintf(stderr, "error: failed to format value\n");
            gen_document_free(doc);
            gen_context_free(ctx);
            free(source);
            return GEN_EXIT_USAGE;
        }
        fprintf(stdout, "%s\n", text);
        gen_string_free(text);
        gen_document_free(doc);
        gen_context_free(ctx);
        free(source);
        return GEN_EXIT_OK;
    }
    if (strcmp(argv[1], "convert") == 0) {
        const char *path = NULL;
        int want_json = 0;
        int from_json = 0;
        int want_yaml = 0;
        int from_yaml = 0;
        int want_binary = 0;
        int from_binary = 0;
        int modes;
        int i;
        GenContext *ctx;
        GenDocument *doc = NULL;
        char *text = NULL;
        size_t text_len = 0;
        char *source;
        size_t source_len = 0;
        int io_error = 0;
        GenResult rc;
        int exit_code;

        for (i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--json") == 0) {
                want_json = 1;
            } else if (strcmp(argv[i], "--from-json") == 0) {
                from_json = 1;
            } else if (strcmp(argv[i], "--yaml") == 0) {
                want_yaml = 1;
            } else if (strcmp(argv[i], "--from-yaml") == 0) {
                from_yaml = 1;
            } else if (strcmp(argv[i], "--binary") == 0) {
                want_binary = 1;
            } else if (strcmp(argv[i], "--from-binary") == 0) {
                from_binary = 1;
            } else if (argv[i][0] == '-') {
                fprintf(stderr, "unknown option '%s'\n", argv[i]);
                return GEN_EXIT_USAGE;
            } else if (path == NULL) {
                path = argv[i];
            } else {
                fputs(
                    "usage: genlang convert <file> --json|--from-json|--yaml|--from-yaml|--binary|--from-binary\n",
                    stderr
                );
                return GEN_EXIT_USAGE;
            }
        }
        modes = want_json + from_json + want_yaml + from_yaml + want_binary + from_binary;
        if (path == NULL || modes != 1) {
            fputs(
                "usage: genlang convert <file> --json|--from-json|--yaml|--from-yaml|--binary|--from-binary\n",
                stderr
            );
            return GEN_EXIT_USAGE;
        }
        source = gen_read_file(path, &io_error, &source_len);
        ctx = gen_context_create();
        if (ctx == NULL) {
            free(source);
            fputs("error: out of memory\n", stderr);
            return GEN_EXIT_USAGE;
        }
        if (source == NULL) {
            fprintf(stderr, "%s: I/O error: cannot read file\n", path);
            gen_context_free(ctx);
            return GEN_EXIT_IO;
        }
        if (from_json) {
            rc = gen_document_from_json(ctx, source, &doc);
        } else if (from_yaml) {
            rc = gen_document_from_yaml(ctx, source, &doc);
        } else if (from_binary) {
            rc = gen_document_from_binary(ctx, source, source_len, &doc);
        } else {
            rc = gen_document_parse_at(ctx, source, path, &doc);
        }
        if (rc != GEN_OK) {
            gen_cli_print_error(path, source, ctx, stderr);
            exit_code = gen_exit_from_result(rc);
            free(source);
            gen_context_free(ctx);
            return exit_code;
        }
        if (want_json) {
            rc = gen_document_to_json(doc, &text, NULL);
        } else if (want_yaml) {
            rc = gen_document_to_yaml(doc, &text, NULL);
        } else if (want_binary) {
            rc = gen_document_to_binary(doc, &text, &text_len);
        } else {
            rc = gen_document_serialize(doc, &text, NULL);
        }
        if (rc != GEN_OK) {
            fprintf(stderr, "error: conversion failed\n");
            gen_document_free(doc);
            gen_context_free(ctx);
            free(source);
            return GEN_EXIT_USAGE;
        }
        if (want_binary) {
            if (fwrite(text, 1, text_len, stdout) != text_len) {
                fprintf(stderr, "error: failed to write binary output\n");
                gen_string_free(text);
                gen_document_free(doc);
                gen_context_free(ctx);
                free(source);
                return GEN_EXIT_IO;
            }
        } else {
            fprintf(stdout, "%s\n", text);
        }
        gen_string_free(text);
        gen_document_free(doc);
        gen_context_free(ctx);
        free(source);
        return GEN_EXIT_OK;
    }
    if (strcmp(argv[1], "format") == 0) {
        const char *path = NULL;
        int in_place = 0;
        int i;
        GenContext *ctx;
        GenDocument *doc = NULL;
        char *text = NULL;
        size_t length = 0;
        char *source;
        int io_error = 0;
        GenResult rc;
        int exit_code;

        for (i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--in-place") == 0 || strcmp(argv[i], "-i") == 0) {
                in_place = 1;
            } else if (argv[i][0] == '-') {
                fprintf(stderr, "unknown option '%s'\n", argv[i]);
                return GEN_EXIT_USAGE;
            } else if (path == NULL) {
                path = argv[i];
            } else {
                fputs("usage: genlang format [--in-place] <file.gl>\n", stderr);
                return GEN_EXIT_USAGE;
            }
        }
        if (path == NULL) {
            fputs("usage: genlang format [--in-place] <file.gl>\n", stderr);
            return GEN_EXIT_USAGE;
        }
        source = gen_read_file(path, &io_error, NULL);
        ctx = gen_context_create();
        if (ctx == NULL) {
            free(source);
            fputs("error: out of memory\n", stderr);
            return GEN_EXIT_USAGE;
        }
        if (source == NULL) {
            fprintf(stderr, "%s: I/O error: cannot read file\n", path);
            gen_context_free(ctx);
            return GEN_EXIT_IO;
        }
        rc = gen_document_parse_at(ctx, source, path, &doc);
        if (rc != GEN_OK) {
            gen_cli_print_error(path, source, ctx, stderr);
            exit_code = gen_exit_from_result(rc);
            free(source);
            gen_context_free(ctx);
            return exit_code;
        }
        rc = gen_document_serialize(doc, &text, &length);
        if (rc != GEN_OK) {
            fprintf(stderr, "error: serialization failed\n");
            gen_document_free(doc);
            gen_context_free(ctx);
            free(source);
            return GEN_EXIT_USAGE;
        }
        if (in_place) {
            rc = gen_document_save_file(ctx, path, doc);
            if (rc != GEN_OK) {
                fprintf(stderr, "%s: I/O error: cannot write file\n", path);
                gen_string_free(text);
                gen_document_free(doc);
                gen_context_free(ctx);
                free(source);
                return GEN_EXIT_IO;
            }
        } else if (fwrite(text, 1, length, stdout) != length || fputc('\n', stdout) == EOF) {
            fprintf(stderr, "error: failed to write formatted output\n");
            gen_string_free(text);
            gen_document_free(doc);
            gen_context_free(ctx);
            free(source);
            return GEN_EXIT_IO;
        }
        gen_string_free(text);
        gen_document_free(doc);
        gen_context_free(ctx);
        free(source);
        return GEN_EXIT_OK;
    }
    if (argv[1][0] == '-') {
        fprintf(stderr, "unknown option '%s'\n", argv[1]);
        gen_cli_print_help(stderr);
        return GEN_EXIT_USAGE;
    }
    return gen_run_file(argv[1], 0);
}
