#include "repl.h"

#include "commands.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#else
#include <unistd.h>
#endif

static int gen_is_complete(const char *s)
{
    int braces = 0;
    int brackets = 0;
    int in_string = 0;
    int escape = 0;
    size_t i;

    for (i = 0; s[i] != '\0'; i++) {
        char c = s[i];
        if (in_string) {
            if (escape) {
                escape = 0;
                continue;
            }
            if (c == '\\') {
                escape = 1;
                continue;
            }
            if (c == '"') {
                in_string = 0;
            }
            continue;
        }
        if (c == '#') {
            break;
        }
        if (c == '"') {
            in_string = 1;
        } else if (c == '{') {
            braces++;
        } else if (c == '}') {
            braces--;
        } else if (c == '[') {
            brackets++;
        } else if (c == ']') {
            brackets--;
        }
    }
    return braces <= 0 && brackets <= 0 && !in_string;
}

static int gen_ident_continue_byte(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '_' || c >= 0x80u;
}

static int gen_keyword_prefix(const char *line, const char *kw)
{
    size_t n = strlen(kw);
    if (strncmp(line, kw, n) != 0) {
        return 0;
    }
    return line[n] == '\0' || !gen_ident_continue_byte((unsigned char)line[n]);
}

static int gen_starts_with_decl(const char *line)
{
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    return gen_keyword_prefix(line, "cins") || gen_keyword_prefix(line, "tur") ||
           gen_keyword_prefix(line, "kume") || gen_keyword_prefix(line, "veri") ||
           gen_keyword_prefix(line, "uye") || gen_keyword_prefix(line, "iceaktar");
}

int gen_cli_repl(const char *path)
{
    GenContext *ctx;
    GenDocument *doc = NULL;
    char *source = NULL;
    size_t source_cap = 0;
    size_t source_len = 0;
    char line[4096];
    char accum[16384];
    size_t accum_len = 0;
    int exit_code = GEN_EXIT_OK;

    ctx = gen_context_create();
    if (ctx == NULL) {
        fputs("error: out of memory\n", stderr);
        return GEN_EXIT_USAGE;
    }

    if (path != NULL) {
        GenResult rc = gen_document_load_file(ctx, path, &doc);
        if (rc != GEN_OK) {
            gen_cli_print_error(path, NULL, ctx, stderr);
            exit_code = gen_exit_from_result(rc);
            gen_context_free(ctx);
            return exit_code;
        }
        fprintf(stdout, "loaded %s\n", path);
    } else {
        GenResult rc = gen_document_parse(ctx, "", &doc);
        if (rc != GEN_OK) {
            gen_cli_print_error("<repl>", "", ctx, stderr);
            gen_context_free(ctx);
            return gen_exit_from_result(rc);
        }
        source = (char *)calloc(1, 1);
        source_cap = 1;
        source_len = 0;
        if (source == NULL) {
            gen_document_free(doc);
            gen_context_free(ctx);
            return GEN_EXIT_USAGE;
        }
    }

    fputs("genlang repl  (yardim, cikis)\n", stdout);
    accum[0] = '\0';

    for (;;) {
        int leave = 0;
        int status;
        const char *prompt = accum_len == 0 ? "genlang> " : "... ";

        fputs(prompt, stdout);
        fflush(stdout);
        if (fgets(line, (int)sizeof(line), stdin) == NULL) {
            fputc('\n', stdout);
            break;
        }
        {
            size_t n = strlen(line);
            if (n + accum_len + 1u >= sizeof(accum)) {
                fputs("error: input too long\n", stderr);
                accum_len = 0;
                accum[0] = '\0';
                continue;
            }
            memcpy(accum + accum_len, line, n + 1u);
            accum_len += n;
        }
        if (!gen_is_complete(accum)) {
            continue;
        }
        while (accum_len > 0 &&
               (accum[accum_len - 1u] == '\n' || accum[accum_len - 1u] == '\r')) {
            accum[--accum_len] = '\0';
        }
        if (accum_len == 0) {
            continue;
        }
        if (gen_starts_with_decl(accum) && path == NULL) {
            GenDocument *next = NULL;
            GenResult rc;
            size_t need = source_len + accum_len + 2u;
            char *grown;
            if (need > source_cap) {
                size_t cap = source_cap == 0 ? 128u : source_cap;
                while (cap < need) {
                    cap *= 2u;
                }
                grown = (char *)realloc(source, cap);
                if (grown == NULL) {
                    fputs("error: out of memory\n", stderr);
                    accum_len = 0;
                    accum[0] = '\0';
                    continue;
                }
                source = grown;
                source_cap = cap;
            }
            if (source_len > 0) {
                source[source_len++] = '\n';
            }
            memcpy(source + source_len, accum, accum_len);
            source_len += accum_len;
            source[source_len] = '\0';
            gen_context_clear_error(ctx);
            rc = gen_document_parse(ctx, source, &next);
            if (rc != GEN_OK) {
                gen_cli_print_error("<repl>", source, ctx, stderr);
                source_len -= accum_len;
                if (source_len > 0 && source[source_len - 1u] == '\n') {
                    source_len--;
                }
                source[source_len] = '\0';
            } else {
                gen_document_free(doc);
                doc = next;
            }
            accum_len = 0;
            accum[0] = '\0';
            continue;
        }

        status = gen_cli_exec_line(doc, accum, stdout, stderr, &leave);
        (void)status;
        accum_len = 0;
        accum[0] = '\0';
        if (leave) {
            break;
        }
    }

    free(source);
    gen_document_free(doc);
    gen_context_free(ctx);
    return exit_code;
}
