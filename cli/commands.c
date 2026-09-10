#include "commands.h"

#include <string.h>

int gen_exit_from_result(GenResult result)
{
    switch (result) {
    case GEN_OK:
        return GEN_EXIT_OK;
    case GEN_ERR_LEX:
    case GEN_ERR_PARSE:
        return GEN_EXIT_PARSE;
    case GEN_ERR_SEMANTIC:
    case GEN_ERR_CYCLE:
    case GEN_ERR_DUPLICATE:
        return GEN_EXIT_SEMANTIC;
    case GEN_ERR_IO:
        return GEN_EXIT_IO;
    default:
        return GEN_EXIT_USAGE;
    }
}

static void gen_cli_print_snippet(const char *source, size_t line, size_t column, FILE *err)
{
    size_t cur_line = 1;
    size_t i = 0;
    size_t start;
    size_t end;
    size_t col;

    if (source == NULL || line == 0) {
        return;
    }
    while (source[i] != '\0' && cur_line < line) {
        if (source[i] == '\n') {
            cur_line++;
        }
        i++;
    }
    if (cur_line != line) {
        return;
    }
    start = i;
    while (source[i] != '\0' && source[i] != '\n') {
        i++;
    }
    end = i;
    fprintf(err, "%zu | %.*s\n", line, (int)(end - start), source + start);
    fputs("  | ", err);
    col = column == 0 ? 1 : column;
    for (i = 1; i < col; i++) {
        fputc(' ', err);
    }
    fputs("^\n", err);
}

static const char *gen_cli_error_kind(GenResult code)
{
    if (code == GEN_ERR_LEX) {
        return "lex error";
    }
    if (code == GEN_ERR_PARSE) {
        return "parse error";
    }
    if (code == GEN_ERR_SEMANTIC || code == GEN_ERR_CYCLE || code == GEN_ERR_DUPLICATE) {
        return "semantic error";
    }
    return "error";
}

static void gen_cli_print_one_error(
    const char *file,
    const char *source,
    const GenError *error,
    FILE *err
)
{
    GenResult code = gen_error_code(error);
    const char *kind = gen_cli_error_kind(code);
    const char *shown = gen_error_path(error);
    int snippet = 0;

    if (shown == NULL || shown[0] == '\0') {
        shown = file;
        snippet = 1;
    } else if (file != NULL && strcmp(shown, file) == 0) {
        snippet = 1;
    }

    if (gen_error_line(error) > 0) {
        fprintf(
            err,
            "%s:%zu:%zu: %s:\n%s\n",
            shown,
            gen_error_line(error),
            gen_error_column(error),
            kind,
            gen_error_message(error)
        );
        if (snippet) {
            gen_cli_print_snippet(source, gen_error_line(error), gen_error_column(error), err);
        }
    } else {
        fprintf(err, "%s: %s:\n%s\n", shown, kind, gen_error_message(error));
    }
}

void gen_cli_print_error(const char *path, const char *source, const GenContext *ctx, FILE *err)
{
    const char *file = path != NULL ? path : "<input>";
    size_t n = gen_context_error_count(ctx);
    size_t i;

    if (n == 0) {
        gen_cli_print_one_error(file, source, gen_context_last_error(ctx), err);
        return;
    }
    for (i = 0; i < n; i++) {
        if (i > 0) {
            fputc('\n', err);
        }
        gen_cli_print_one_error(file, source, gen_context_error(ctx, i), err);
    }
    if (n > 1) {
        fprintf(err, "\n%d errors\n", (int)n);
    }
}

void gen_cli_print_help(FILE *out)
{
    fputs(
        "genlang — CLI frontend for libgenlang\n"
        "\n"
        "Usage:\n"
        "  genlang <file.gl>                 Parse, validate, and summarize a document\n"
        "  genlang check <file.gl>           Validate syntax and semantics\n"
        "  genlang query <file.gl> <path>    Evaluate a nested path\n"
        "  genlang convert <file.gl> --json    Emit JSON (types, sets, values, memberships)\n"
        "  genlang convert <file.json> --from-json  Import that JSON back to GenLang\n"
        "  genlang convert <file.gl> --yaml    Emit YAML with the same schema as JSON\n"
        "  genlang convert <file.yaml> --from-yaml  Import that YAML back to GenLang\n"
        "  genlang convert <file.gl> --binary  Emit compact binary\n"
        "  genlang convert <file.bin> --from-binary  Import that binary back to GenLang\n"
        "  genlang format <file.gl>          Print canonical GenLang\n"
        "  genlang format --in-place <file>  Rewrite the file in canonical form\n"
        "  genlang repl [file.gl]            Interactive query shell\n"
        "  genlang --help                    Show this help\n"
        "  genlang --version                 Show version\n"
        "\n"
        "Exit codes:\n"
        "  0  success\n"
        "  1  general / usage error\n"
        "  2  lex / parse error\n"
        "  3  semantic error\n"
        "  4  I/O error\n"
        "\n"
        "The library (libgenlang) is the product; this executable only consumes the C API.\n",
        out
    );
}

int gen_cli_print_document(const char *path, const GenDocument *doc, FILE *out)
{
    size_t i;
    const char *name;

    fprintf(out, "OK %s\n", path != NULL ? path : "<memory>");
    fprintf(out, "  types:    %zu\n", gen_type_count(doc));
    fprintf(out, "  sets:     %zu\n", gen_set_count(doc));
    fprintf(out, "  entities: %zu\n", gen_entity_count(doc));
    if (gen_type_count(doc) > 0) {
        fputs("  type names:\n", out);
        for (i = 0; i < gen_type_count(doc); i++) {
            if (gen_type_name(doc, i, &name) == GEN_OK) {
                fprintf(out, "    %s\n", name);
            }
        }
    }
    if (gen_set_count(doc) > 0) {
        fputs("  set names:\n", out);
        for (i = 0; i < gen_set_count(doc); i++) {
            if (gen_set_name(doc, i, &name) == GEN_OK) {
                fprintf(out, "    %s\n", name);
            }
        }
    }
    if (gen_entity_count(doc) > 0) {
        fputs("  entity names:\n", out);
        for (i = 0; i < gen_entity_count(doc); i++) {
            if (gen_entity_name(doc, i, &name) == GEN_OK) {
                fprintf(out, "    %s\n", name);
            }
        }
    }
    return GEN_EXIT_OK;
}

static void gen_print_result_lines(const GenQueryResult *result, FILE *out)
{
    size_t i;
    size_t n = gen_query_result_count(result);
    if (n == 0) {
        fputs("(none)\n", out);
        return;
    }
    for (i = 0; i < n; i++) {
        fprintf(out, "%s\n", gen_query_result_name(result, i));
    }
}

static int gen_first_keyword(const char *line, char *kw, size_t kw_size)
{
    size_t i = 0;
    size_t n = 0;

    while (line[i] == ' ' || line[i] == '\t') {
        i++;
    }
    while (line[i] != '\0' && line[i] != ' ' && line[i] != '\t' && n + 1u < kw_size) {
        kw[n++] = line[i++];
    }
    kw[n] = '\0';
    return n > 0;
}

static const char *gen_skip_kw(const char *line)
{
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    while (*line != '\0' && *line != ' ' && *line != '\t') {
        line++;
    }
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    return line;
}

static void gen_copy_word(const char *in, char *out, size_t out_size)
{
    size_t n = 0;
    while (*in == ' ' || *in == '\t') {
        in++;
    }
    while (*in != '\0' && *in != ' ' && *in != '\t' && *in != '\n' && n + 1u < out_size) {
        out[n++] = *in++;
    }
    out[n] = '\0';
}

static void gen_unquote(char *s)
{
    size_t n;
    if (s[0] != '"') {
        return;
    }
    n = strlen(s);
    if (n >= 2u && s[n - 1u] == '"') {
        memmove(s, s + 1, n - 2u);
        s[n - 2u] = '\0';
    }
}

int gen_cli_exec_line(GenDocument *doc, const char *line, FILE *out, FILE *err, int *out_exit_repl)
{
    char kw[32];
    char arg1[256];
    char arg2[256];
    const char *rest;
    GenResult rc;
    GenQueryResult *result = NULL;

    if (out_exit_repl != NULL) {
        *out_exit_repl = 0;
    }
    if (!gen_first_keyword(line, kw, sizeof(kw))) {
        return GEN_EXIT_OK;
    }

    if (strcmp(kw, "yardim") == 0) {
        fputs(
            "REPL commands:\n"
            "  yardim                      Show this help\n"
            "  temizle                     Clear the screen\n"
            "  cikis                       Exit\n"
            "  dyaz <entity>               Types and sets of an entity\n"
            "  goster <entity>             Full value of an entity\n"
            "  uyeler <set>                Members of a set\n"
            "  icerir <set> <entity>       Membership test\n"
            "  ustler <type>               Ancestors of a type\n"
            "  altlar <type>               Descendants of a type\n"
            "  yol <from> <to>             Path in the type hierarchy\n"
            "  ara <text>                  Search names and string values\n"
            "  liste cins|tur|kume|veri    List declarations\n"
            "  <path>                      Evaluate a nested path (e.g. x.a[1].b[2])\n",
            out
        );
        return GEN_EXIT_OK;
    }
    if (strcmp(kw, "temizle") == 0) {
        fputs("\033[2J\033[H", out);
        return GEN_EXIT_OK;
    }
    if (strcmp(kw, "cikis") == 0) {
        if (out_exit_repl != NULL) {
            *out_exit_repl = 1;
        }
        return GEN_EXIT_OK;
    }

    rest = gen_skip_kw(line);
    gen_copy_word(rest, arg1, sizeof(arg1));
    gen_copy_word(gen_skip_kw(rest), arg2, sizeof(arg2));

    if (strcmp(kw, "dyaz") == 0) {
        char *text = NULL;
        rc = gen_format_dyaz(doc, arg1, &text);
        if (rc != GEN_OK) {
            fprintf(err, "error: unknown entity '%s'\n", arg1);
            return GEN_EXIT_USAGE;
        }
        fputs(text, out);
        gen_string_free(text);
        return GEN_EXIT_OK;
    }
    if (strcmp(kw, "goster") == 0) {
        char *text = NULL;
        rc = gen_format_goster(doc, arg1, &text);
        if (rc != GEN_OK) {
            fprintf(err, "error: unknown entity '%s'\n", arg1);
            return GEN_EXIT_USAGE;
        }
        fputs(text, out);
        gen_string_free(text);
        return GEN_EXIT_OK;
    }
    if (strcmp(kw, "uyeler") == 0) {
        rc = gen_set_members(doc, arg1, &result);
        if (rc != GEN_OK) {
            fprintf(err, "error: unknown set '%s'\n", arg1);
            return GEN_EXIT_USAGE;
        }
        gen_print_result_lines(result, out);
        gen_query_result_free(result);
        return GEN_EXIT_OK;
    }
    if (strcmp(kw, "icerir") == 0) {
        int is_member = 0;
        rc = gen_is_member(doc, arg1, arg2, &is_member);
        if (rc != GEN_OK) {
            fprintf(err, "error: unknown set '%s'\n", arg1);
            return GEN_EXIT_USAGE;
        }
        fputs(is_member ? "true\n" : "false\n", out);
        return GEN_EXIT_OK;
    }
    if (strcmp(kw, "ustler") == 0) {
        rc = gen_ancestors_of(doc, arg1, &result);
        if (rc != GEN_OK) {
            fprintf(err, "error: unknown type '%s'\n", arg1);
            return GEN_EXIT_USAGE;
        }
        gen_print_result_lines(result, out);
        gen_query_result_free(result);
        return GEN_EXIT_OK;
    }
    if (strcmp(kw, "altlar") == 0) {
        rc = gen_descendants_of(doc, arg1, &result);
        if (rc != GEN_OK) {
            fprintf(err, "error: unknown type '%s'\n", arg1);
            return GEN_EXIT_USAGE;
        }
        gen_print_result_lines(result, out);
        gen_query_result_free(result);
        return GEN_EXIT_OK;
    }
    if (strcmp(kw, "yol") == 0) {
        size_t i;
        rc = gen_type_path(doc, arg1, arg2, &result);
        if (rc != GEN_OK) {
            fprintf(err, "error: no type path between '%s' and '%s'\n", arg1, arg2);
            return GEN_EXIT_USAGE;
        }
        for (i = 0; i < gen_query_result_count(result); i++) {
            if (i > 0) {
                fputs(" -> ", out);
            }
            fputs(gen_query_result_name(result, i), out);
        }
        fputc('\n', out);
        gen_query_result_free(result);
        return GEN_EXIT_OK;
    }
    if (strcmp(kw, "ara") == 0) {
        gen_unquote(arg1);
        rc = gen_search(doc, arg1, &result);
        if (rc != GEN_OK) {
            fprintf(err, "error: search failed\n");
            return GEN_EXIT_USAGE;
        }
        gen_print_result_lines(result, out);
        gen_query_result_free(result);
        return GEN_EXIT_OK;
    }
    if (strcmp(kw, "liste") == 0) {
        size_t i;
        const char *name;
        if (strcmp(arg1, "cins") == 0) {
            for (i = 0; i < gen_type_count(doc); i++) {
                GenTypeKind kind;
                if (gen_type_kind(doc, i, &kind) == GEN_OK && kind == GEN_TYPE_KIND_GENUS &&
                    gen_type_name(doc, i, &name) == GEN_OK) {
                    fprintf(out, "%s\n", name);
                }
            }
        } else if (strcmp(arg1, "tur") == 0) {
            for (i = 0; i < gen_type_count(doc); i++) {
                GenTypeKind kind;
                if (gen_type_kind(doc, i, &kind) == GEN_OK && kind == GEN_TYPE_KIND_SPECIES &&
                    gen_type_name(doc, i, &name) == GEN_OK) {
                    fprintf(out, "%s\n", name);
                }
            }
        } else if (strcmp(arg1, "kume") == 0) {
            for (i = 0; i < gen_set_count(doc); i++) {
                if (gen_set_name(doc, i, &name) == GEN_OK) {
                    fprintf(out, "%s\n", name);
                }
            }
        } else if (strcmp(arg1, "veri") == 0) {
            for (i = 0; i < gen_entity_count(doc); i++) {
                if (gen_entity_name(doc, i, &name) == GEN_OK) {
                    fprintf(out, "%s\n", name);
                }
            }
        } else {
            fprintf(err, "error: liste expects cins, tur, kume, or veri\n");
            return GEN_EXIT_USAGE;
        }
        return GEN_EXIT_OK;
    }

    if (strcmp(kw, "cins") == 0 || strcmp(kw, "tur") == 0 || strcmp(kw, "kume") == 0 ||
        strcmp(kw, "veri") == 0 || strcmp(kw, "uye") == 0) {
        fprintf(err, "error: declarations must be loaded from a file in this session\n");
        return GEN_EXIT_USAGE;
    }

    {
        GenValue *value = NULL;
        char *text = NULL;
        rc = gen_eval_path(doc, line, &value);
        if (rc != GEN_OK) {
            fprintf(err, "error: cannot evaluate path '%s'\n", line);
            return GEN_EXIT_USAGE;
        }
        rc = gen_value_format(value, &text, NULL);
        gen_value_free(value);
        if (rc != GEN_OK) {
            fprintf(err, "error: failed to format value\n");
            return GEN_EXIT_USAGE;
        }
        fprintf(out, "%s\n", text);
        gen_string_free(text);
        return GEN_EXIT_OK;
    }
}
