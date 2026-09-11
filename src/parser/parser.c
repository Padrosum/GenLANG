#include "parser/parser.h"

#include "error/error.h"
#include "internal/string.h"

typedef struct {
    GenContext *ctx;
    const char *source;
    size_t source_len;
    GenToken *tokens;
    size_t token_count;
    size_t index;
    GenArena *arena;
    size_t depth;
    GenParseMode mode;
    const char *origin;
} GenParser;

static const GenToken *gen_cur(const GenParser *p)
{
    if (p->index >= p->token_count) {
        return &p->tokens[p->token_count - 1u];
    }
    return &p->tokens[p->index];
}

static bool gen_check(const GenParser *p, GenTokenKind kind)
{
    return gen_cur(p)->kind == kind;
}

static const GenToken *gen_advance(GenParser *p)
{
    const GenToken *tok = gen_cur(p);
    if (tok->kind != TOK_EOF && p->index + 1u < p->token_count) {
        p->index++;
    }
    return tok;
}

static bool gen_match(GenParser *p, GenTokenKind kind)
{
    if (gen_check(p, kind)) {
        gen_advance(p);
        return true;
    }
    return false;
}

static GenResult gen_fail(GenParser *p, GenResult code, const char *fmt, ...)
{
    va_list args;
    const GenToken *tok = gen_cur(p);

    va_start(args, fmt);
    gen_context_set_errorv(p->ctx, code, tok->line, tok->column, tok->offset, fmt, args);
    va_end(args);
    return code;
}

static bool gen_enter(GenParser *p)
{
    if (p->depth >= p->ctx->limits.max_nesting_depth) {
        gen_fail(p, GEN_ERR_PARSE, "maximum nesting depth exceeded");
        return false;
    }
    p->depth++;
    return true;
}

static void gen_leave(GenParser *p)
{
    if (p->depth > 0) {
        p->depth--;
    }
}

static char *gen_copy_ident(GenParser *p, const GenToken *tok)
{
    return gen_arena_strndup(p->arena, tok->lexeme, tok->length);
}

static GenAst *gen_node(GenParser *p, GenAstKind kind, const GenToken *tok)
{
    GenAst *node = gen_ast_new(p->arena, kind, tok);
    if (node == NULL) {
        return NULL;
    }
    if (p->origin != NULL) {
        node->origin = gen_arena_strdup(p->arena, p->origin);
        if (node->origin == NULL) {
            return NULL;
        }
    }
    return node;
}

static bool gen_unescape_string(
    GenParser *p,
    const GenToken *tok,
    char **out_data,
    size_t *out_len
)
{
    const char *src;
    size_t n;
    size_t i;
    size_t w;
    char *buf;

    if (tok->length < 2u) {
        gen_fail(p, GEN_ERR_LEX, "invalid string literal");
        return false;
    }
    src = tok->lexeme + 1;
    n = tok->length - 2u;
    buf = (char *)gen_arena_alloc(p->arena, n + 1u);
    if (buf == NULL) {
        gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
        return false;
    }
    w = 0;
    for (i = 0; i < n; i++) {
        if (src[i] == '\\' && i + 1u < n) {
            char e = src[i + 1u];
            if (e == 'n') {
                buf[w++] = '\n';
            } else if (e == 't') {
                buf[w++] = '\t';
            } else if (e == 'r') {
                buf[w++] = '\r';
            } else {
                buf[w++] = e;
            }
            i++;
        } else {
            buf[w++] = src[i];
        }
    }
    buf[w] = '\0';
    if (!gen_utf8_validate(buf, w)) {
        gen_context_set_error(
            p->ctx, GEN_ERR_LEX, tok->line, tok->column, tok->offset, "string is not valid UTF-8"
        );
        return false;
    }
    *out_data = buf;
    *out_len = w;
    return true;
}

static GenAst *gen_parse_expr(GenParser *p);
static GenAst *gen_parse_object(GenParser *p);
static GenAst *gen_parse_list(GenParser *p);
static GenAst *gen_parse_path_from_ident(GenParser *p, const GenToken *ident);

static bool gen_is_name_token(const GenParser *p)
{
    GenTokenKind kind = gen_cur(p)->kind;
    return kind == TOK_IDENT || gen_token_is_keyword(kind);
}

static GenAst *gen_parse_literal(GenParser *p)
{
    const GenToken *tok = gen_cur(p);
    GenAst *node;

    switch (tok->kind) {
    case TOK_STRING:
        node = gen_node(p, GEN_AST_STRING, tok);
        if (node == NULL) {
            gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
            return NULL;
        }
        if (!gen_unescape_string(p, tok, &node->u.string.data, &node->u.string.length)) {
            return NULL;
        }
        gen_advance(p);
        return node;
    case TOK_INT:
        node = gen_node(p, GEN_AST_INT, tok);
        if (node == NULL) {
            gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
            return NULL;
        }
        node->u.integer = tok->int_value;
        gen_advance(p);
        return node;
    case TOK_FLOAT:
        node = gen_node(p, GEN_AST_FLOAT, tok);
        if (node == NULL) {
            gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
            return NULL;
        }
        node->u.floating = tok->float_value;
        gen_advance(p);
        return node;
    case TOK_TRUE:
    case TOK_FALSE:
        node = gen_node(p, GEN_AST_BOOL, tok);
        if (node == NULL) {
            gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
            return NULL;
        }
        node->u.boolean = tok->kind == TOK_TRUE;
        gen_advance(p);
        return node;
    case TOK_NULL:
        node = gen_node(p, GEN_AST_NULL, tok);
        if (node == NULL) {
            gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
            return NULL;
        }
        gen_advance(p);
        return node;
    case TOK_AT: {
        const GenToken *name;
        gen_advance(p);
        if (!gen_check(p, TOK_IDENT)) {
            gen_fail(p, GEN_ERR_PARSE, "expected identifier after '@'");
            return NULL;
        }
        name = gen_cur(p);
        node = gen_node(p, GEN_AST_REFERENCE, name);
        if (node == NULL) {
            gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
            return NULL;
        }
        node->u.reference.name = gen_copy_ident(p, name);
        if (node->u.reference.name == NULL) {
            gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
            return NULL;
        }
        gen_advance(p);
        return node;
    }
    default:
        gen_fail(p, GEN_ERR_PARSE, "expected value, found %s", gen_token_kind_name(tok->kind));
        return NULL;
    }
}

static GenAst *gen_parse_list(GenParser *p)
{
    const GenToken *open = gen_cur(p);
    GenAst *node;
    GenAst **items = NULL;
    size_t count = 0;
    size_t cap = 0;

    if (!gen_enter(p)) {
        return NULL;
    }
    gen_advance(p);
    node = gen_node(p, GEN_AST_LIST, open);
    if (node == NULL) {
        gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
        gen_leave(p);
        return NULL;
    }
    if (!gen_check(p, TOK_RBRACKET)) {
        for (;;) {
            GenAst *item = gen_parse_expr(p);
            GenAst **grown;
            if (item == NULL) {
                gen_leave(p);
                return NULL;
            }
            if (count >= p->ctx->limits.max_list_length) {
                gen_fail(p, GEN_ERR_PARSE, "list exceeds maximum length");
                gen_leave(p);
                return NULL;
            }
            if (count + 1u > cap) {
                cap = cap == 0 ? 4u : cap * 2u;
                grown = (GenAst **)gen_arena_alloc(p->arena, cap * sizeof(GenAst *));
                if (grown == NULL) {
                    gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
                    gen_leave(p);
                    return NULL;
                }
                if (items != NULL && count > 0) {
                    memcpy(grown, items, count * sizeof(GenAst *));
                }
                items = grown;
            }
            items[count++] = item;
            if (gen_match(p, TOK_COMMA)) {
                if (gen_check(p, TOK_RBRACKET)) {
                    break;
                }
                continue;
            }
            break;
        }
    }
    if (!gen_match(p, TOK_RBRACKET)) {
        gen_fail(p, GEN_ERR_PARSE, "expected ']'");
        gen_leave(p);
        return NULL;
    }
    node->u.list.items = items;
    node->u.list.count = count;
    gen_leave(p);
    return node;
}

static GenAst *gen_parse_object(GenParser *p)
{
    const GenToken *open = gen_cur(p);
    GenAst *node;
    char **keys = NULL;
    GenAst **values = NULL;
    size_t count = 0;
    size_t cap = 0;

    if (!gen_enter(p)) {
        return NULL;
    }
    gen_advance(p);
    node = gen_node(p, GEN_AST_OBJECT, open);
    if (node == NULL) {
        gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
        gen_leave(p);
        return NULL;
    }
    while (!gen_check(p, TOK_RBRACE) && !gen_check(p, TOK_EOF)) {
        const GenToken *key_tok;
        char *key;
        GenAst *value;
        size_t i;
        char **new_keys;
        GenAst **new_values;

        if (!gen_is_name_token(p)) {
            gen_fail(p, GEN_ERR_PARSE, "expected property name");
            gen_leave(p);
            return NULL;
        }
        key_tok = gen_cur(p);
        key = gen_copy_ident(p, key_tok);
        if (key == NULL) {
            gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
            gen_leave(p);
            return NULL;
        }
        gen_advance(p);
        if (!gen_match(p, TOK_EQ)) {
            gen_fail(p, GEN_ERR_PARSE, "expected '=' after property name");
            gen_leave(p);
            return NULL;
        }
        value = gen_parse_expr(p);
        if (value == NULL) {
            gen_leave(p);
            return NULL;
        }
        for (i = 0; i < count; i++) {
            if (strcmp(keys[i], key) == 0) {
                gen_context_set_error(
                    p->ctx,
                    GEN_ERR_DUPLICATE,
                    key_tok->line,
                    key_tok->column,
                    key_tok->offset,
                    "duplicate property '%s'",
                    key
                );
                gen_leave(p);
                return NULL;
            }
        }
        if (count >= p->ctx->limits.max_object_properties) {
            gen_fail(p, GEN_ERR_PARSE, "object exceeds maximum property count");
            gen_leave(p);
            return NULL;
        }
        if (count + 1u > cap) {
            cap = cap == 0 ? 4u : cap * 2u;
            new_keys = (char **)gen_arena_alloc(p->arena, cap * sizeof(char *));
            new_values = (GenAst **)gen_arena_alloc(p->arena, cap * sizeof(GenAst *));
            if (new_keys == NULL || new_values == NULL) {
                gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
                gen_leave(p);
                return NULL;
            }
            if (keys != NULL && count > 0) {
                memcpy(new_keys, keys, count * sizeof(char *));
                memcpy(new_values, values, count * sizeof(GenAst *));
            }
            keys = new_keys;
            values = new_values;
        }
        keys[count] = key;
        values[count] = value;
        count++;
        (void)gen_match(p, TOK_COMMA);
    }
    if (!gen_match(p, TOK_RBRACE)) {
        gen_fail(p, GEN_ERR_PARSE, "expected '}'");
        gen_leave(p);
        return NULL;
    }
    node->u.object.keys = keys;
    node->u.object.values = values;
    node->u.object.count = count;
    gen_leave(p);
    return node;
}

static GenAst *gen_parse_path_from_ident(GenParser *p, const GenToken *ident)
{
    GenAst *node = gen_node(p, GEN_AST_IDENT, ident);
    if (node == NULL) {
        gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
        return NULL;
    }
    node->u.ident.name = gen_copy_ident(p, ident);
    if (node->u.ident.name == NULL) {
        gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
        return NULL;
    }
    gen_advance(p);
    while (gen_check(p, TOK_DOT) || gen_check(p, TOK_LBRACKET)) {
        if (!gen_enter(p)) {
            return NULL;
        }
        if (gen_match(p, TOK_DOT)) {
            const GenToken *prop;
            GenAst *access;
            if (!gen_is_name_token(p)) {
                gen_fail(p, GEN_ERR_PARSE, "expected property name after '.'");
                gen_leave(p);
                return NULL;
            }
            prop = gen_cur(p);
            access = gen_node(p, GEN_AST_PROP_ACCESS, prop);
            if (access == NULL) {
                gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
                gen_leave(p);
                return NULL;
            }
            access->u.prop_access.base = node;
            access->u.prop_access.property = gen_copy_ident(p, prop);
            if (access->u.prop_access.property == NULL) {
                gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
                gen_leave(p);
                return NULL;
            }
            gen_advance(p);
            node = access;
        } else {
            const GenToken *open = gen_cur(p);
            const GenToken *idx;
            GenAst *access;
            gen_advance(p);
            if (!gen_check(p, TOK_INT)) {
                gen_fail(p, GEN_ERR_PARSE, "expected integer index");
                gen_leave(p);
                return NULL;
            }
            idx = gen_cur(p);
            if (idx->int_value < 0) {
                gen_context_set_error(
                    p->ctx,
                    GEN_ERR_INDEX,
                    idx->line,
                    idx->column,
                    idx->offset,
                    "negative list index"
                );
                gen_leave(p);
                return NULL;
            }
            access = gen_node(p, GEN_AST_INDEX_ACCESS, open);
            if (access == NULL) {
                gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
                gen_leave(p);
                return NULL;
            }
            access->u.index_access.base = node;
            access->u.index_access.index = idx->int_value;
            access->u.index_access.index_line = idx->line;
            access->u.index_access.index_column = idx->column;
            gen_advance(p);
            if (!gen_match(p, TOK_RBRACKET)) {
                gen_fail(p, GEN_ERR_PARSE, "expected ']'");
                gen_leave(p);
                return NULL;
            }
            node = access;
        }
        gen_leave(p);
    }
    return node;
}

static GenAst *gen_parse_expr(GenParser *p)
{
    if (gen_check(p, TOK_LBRACE)) {
        return gen_parse_object(p);
    }
    if (gen_check(p, TOK_LBRACKET)) {
        return gen_parse_list(p);
    }
    if (gen_check(p, TOK_IDENT)) {
        /* Bare identifiers are not values in data position; paths are REPL-only. */
        if (p->mode == GEN_PARSE_REPL) {
            return gen_parse_path_from_ident(p, gen_cur(p));
        }
        gen_fail(p, GEN_ERR_PARSE, "bare identifier is not a value; use @name for a reference");
        return NULL;
    }
    return gen_parse_literal(p);
}

static GenAst *gen_parse_optional_object(GenParser *p)
{
    if (gen_check(p, TOK_LBRACE)) {
        return gen_parse_object(p);
    }
    return NULL;
}

static char *gen_parse_optional_parent(GenParser *p)
{
    const GenToken *tok;
    char *name;

    if (!gen_match(p, TOK_ARROW)) {
        return NULL;
    }
    if (!gen_check(p, TOK_IDENT)) {
        gen_fail(p, GEN_ERR_PARSE, "expected parent type name after '->'");
        return (char *)-1;
    }
    tok = gen_cur(p);
    name = gen_copy_ident(p, tok);
    gen_advance(p);
    if (name == NULL) {
        gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
        return (char *)-1;
    }
    return name;
}

static GenAst *gen_parse_type_decl(GenParser *p, GenAstKind kind)
{
    const GenToken *kw = gen_cur(p);
    const GenToken *name_tok;
    GenAst *node;
    char *parent;

    gen_advance(p);
    if (!gen_check(p, TOK_IDENT)) {
        gen_fail(p, GEN_ERR_PARSE, "expected type name");
        return NULL;
    }
    name_tok = gen_cur(p);
    node = gen_node(p, kind, kw);
    if (node == NULL) {
        gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
        return NULL;
    }
    node->u.type_decl.name = gen_copy_ident(p, name_tok);
    if (node->u.type_decl.name == NULL) {
        gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
        return NULL;
    }
    gen_advance(p);
    parent = gen_parse_optional_parent(p);
    if (parent == (char *)-1) {
        return NULL;
    }
    node->u.type_decl.parent = parent;
    node->u.type_decl.props = gen_parse_optional_object(p);
    if (p->ctx->last_error.code != GEN_OK) {
        return NULL;
    }
    return node;
}

static GenAst *gen_parse_set_decl(GenParser *p)
{
    const GenToken *kw = gen_cur(p);
    const GenToken *name_tok;
    GenAst *node;

    gen_advance(p);
    if (!gen_check(p, TOK_IDENT)) {
        gen_fail(p, GEN_ERR_PARSE, "expected set name");
        return NULL;
    }
    name_tok = gen_cur(p);
    node = gen_node(p, GEN_AST_SET, kw);
    if (node == NULL) {
        gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
        return NULL;
    }
    node->u.set_decl.name = gen_copy_ident(p, name_tok);
    if (node->u.set_decl.name == NULL) {
        gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
        return NULL;
    }
    gen_advance(p);
    node->u.set_decl.props = gen_parse_optional_object(p);
    if (p->ctx->last_error.code != GEN_OK) {
        return NULL;
    }
    return node;
}

static GenAst *gen_parse_data_decl(GenParser *p)
{
    const GenToken *kw = gen_cur(p);
    const GenToken *name_tok;
    GenAst *node;

    gen_advance(p);
    if (!gen_check(p, TOK_IDENT)) {
        gen_fail(p, GEN_ERR_PARSE, "expected entity name");
        return NULL;
    }
    name_tok = gen_cur(p);
    node = gen_node(p, GEN_AST_DATA, kw);
    if (node == NULL) {
        gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
        return NULL;
    }
    node->u.data_decl.name = gen_copy_ident(p, name_tok);
    if (node->u.data_decl.name == NULL) {
        gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
        return NULL;
    }
    gen_advance(p);
    if (gen_match(p, TOK_COLON)) {
        const GenToken *type_tok;
        if (!gen_check(p, TOK_IDENT)) {
            gen_fail(p, GEN_ERR_PARSE, "expected type name after ':'");
            return NULL;
        }
        type_tok = gen_cur(p);
        node->u.data_decl.type = gen_copy_ident(p, type_tok);
        if (node->u.data_decl.type == NULL) {
            gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
            return NULL;
        }
        gen_advance(p);
    }
    if (gen_check(p, TOK_LBRACE)) {
        node->u.data_decl.value = gen_parse_object(p);
    } else if (gen_match(p, TOK_EQ)) {
        node->u.data_decl.value = gen_parse_expr(p);
    } else {
        gen_fail(p, GEN_ERR_PARSE, "expected '=' or '{' in data declaration");
        return NULL;
    }
    if (node->u.data_decl.value == NULL) {
        return NULL;
    }
    return node;
}

static GenAst *gen_parse_membership(GenParser *p)
{
    const GenToken *kw = gen_cur(p);
    const GenToken *ent;
    const GenToken *set;
    GenAst *node;

    gen_advance(p);
    if (!gen_check(p, TOK_IDENT)) {
        gen_fail(p, GEN_ERR_PARSE, "expected entity name after 'uye'");
        return NULL;
    }
    ent = gen_cur(p);
    gen_advance(p);
    if (!gen_match(p, TOK_ARROW)) {
        gen_fail(p, GEN_ERR_PARSE, "expected '->' in membership declaration");
        return NULL;
    }
    if (!gen_check(p, TOK_IDENT)) {
        gen_fail(p, GEN_ERR_PARSE, "expected set name after '->'");
        return NULL;
    }
    set = gen_cur(p);
    node = gen_node(p, GEN_AST_MEMBERSHIP, kw);
    if (node == NULL) {
        gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
        return NULL;
    }
    node->u.membership.entity = gen_copy_ident(p, ent);
    node->u.membership.set = gen_copy_ident(p, set);
    if (node->u.membership.entity == NULL || node->u.membership.set == NULL) {
        gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
        return NULL;
    }
    gen_advance(p);
    return node;
}

static GenAst *gen_parse_import(GenParser *p)
{
    const GenToken *kw = gen_cur(p);
    const GenToken *path_tok;
    GenAst *node;

    gen_advance(p);
    if (!gen_check(p, TOK_STRING)) {
        gen_fail(p, GEN_ERR_PARSE, "expected string path after 'iceaktar'");
        return NULL;
    }
    path_tok = gen_cur(p);
    node = gen_node(p, GEN_AST_IMPORT, kw);
    if (node == NULL) {
        gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
        return NULL;
    }
    if (!gen_unescape_string(p, path_tok, &node->u.import.path, &node->u.import.path_length)) {
        return NULL;
    }
    gen_advance(p);
    return node;
}

static bool gen_expect_ident_arg(GenParser *p, char **out)
{
    const GenToken *tok;
    if (!gen_check(p, TOK_IDENT)) {
        gen_fail(p, GEN_ERR_PARSE, "expected identifier");
        return false;
    }
    tok = gen_cur(p);
    *out = gen_copy_ident(p, tok);
    if (*out == NULL) {
        gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
        return false;
    }
    gen_advance(p);
    return true;
}

static GenAst *gen_parse_command(GenParser *p)
{
    const GenToken *kw = gen_cur(p);
    GenAst *node = gen_node(p, GEN_AST_COMMAND, kw);
    if (node == NULL) {
        gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
        return NULL;
    }
    switch (kw->kind) {
    case TOK_DYAZ:
        node->u.command.cmd = GEN_CMD_DYAZ;
        gen_advance(p);
        if (!gen_expect_ident_arg(p, &node->u.command.arg1)) {
            return NULL;
        }
        break;
    case TOK_GOSTER:
        node->u.command.cmd = GEN_CMD_GOSTER;
        gen_advance(p);
        if (!gen_expect_ident_arg(p, &node->u.command.arg1)) {
            return NULL;
        }
        break;
    case TOK_UYELER:
        node->u.command.cmd = GEN_CMD_UYELER;
        gen_advance(p);
        if (!gen_expect_ident_arg(p, &node->u.command.arg1)) {
            return NULL;
        }
        break;
    case TOK_ICERIR:
        node->u.command.cmd = GEN_CMD_ICERIR;
        gen_advance(p);
        if (!gen_expect_ident_arg(p, &node->u.command.arg1)) {
            return NULL;
        }
        if (!gen_expect_ident_arg(p, &node->u.command.arg2)) {
            return NULL;
        }
        break;
    case TOK_USTLER:
        node->u.command.cmd = GEN_CMD_USTLER;
        gen_advance(p);
        if (!gen_expect_ident_arg(p, &node->u.command.arg1)) {
            return NULL;
        }
        break;
    case TOK_ALTLAR:
        node->u.command.cmd = GEN_CMD_ALTLAR;
        gen_advance(p);
        if (!gen_expect_ident_arg(p, &node->u.command.arg1)) {
            return NULL;
        }
        break;
    case TOK_YOL:
        node->u.command.cmd = GEN_CMD_YOL;
        gen_advance(p);
        if (!gen_expect_ident_arg(p, &node->u.command.arg1)) {
            return NULL;
        }
        if (!gen_expect_ident_arg(p, &node->u.command.arg2)) {
            return NULL;
        }
        break;
    case TOK_ARA:
        node->u.command.cmd = GEN_CMD_ARA;
        gen_advance(p);
        if (gen_check(p, TOK_STRING)) {
            char *data;
            size_t len;
            const GenToken *s = gen_cur(p);
            if (!gen_unescape_string(p, s, &data, &len)) {
                return NULL;
            }
            node->u.command.arg1 = data;
            gen_advance(p);
        } else if (!gen_expect_ident_arg(p, &node->u.command.arg1)) {
            return NULL;
        }
        break;
    case TOK_LISTE:
        node->u.command.cmd = GEN_CMD_LISTE;
        gen_advance(p);
        if (gen_check(p, TOK_CINS) || gen_check(p, TOK_TUR) || gen_check(p, TOK_KUME) ||
            gen_check(p, TOK_VERI) || gen_check(p, TOK_IDENT)) {
            const GenToken *arg = gen_cur(p);
            node->u.command.arg1 = gen_copy_ident(p, arg);
            if (node->u.command.arg1 == NULL) {
                gen_fail(p, GEN_ERR_OUT_OF_MEMORY, "out of memory");
                return NULL;
            }
            gen_advance(p);
        } else {
            gen_fail(p, GEN_ERR_PARSE, "expected cins, tur, kume, or veri after 'liste'");
            return NULL;
        }
        break;
    case TOK_YARDIM:
        node->u.command.cmd = GEN_CMD_YARDIM;
        gen_advance(p);
        break;
    case TOK_TEMIZLE:
        node->u.command.cmd = GEN_CMD_TEMIZLE;
        gen_advance(p);
        break;
    case TOK_CIKIS:
        node->u.command.cmd = GEN_CMD_CIKIS;
        gen_advance(p);
        break;
    default:
        gen_fail(p, GEN_ERR_PARSE, "unknown command");
        return NULL;
    }
    return node;
}

static bool gen_is_command_kw(GenTokenKind kind)
{
    return kind == TOK_DYAZ || kind == TOK_GOSTER || kind == TOK_UYELER || kind == TOK_ICERIR ||
           kind == TOK_USTLER || kind == TOK_ALTLAR || kind == TOK_YOL || kind == TOK_ARA ||
           kind == TOK_LISTE || kind == TOK_YARDIM || kind == TOK_TEMIZLE || kind == TOK_CIKIS;
}

static GenAst *gen_parse_statement(GenParser *p)
{
    const GenToken *tok = gen_cur(p);

    if (p->mode == GEN_PARSE_PATH) {
        if (tok->kind == TOK_IDENT) {
            return gen_parse_path_from_ident(p, tok);
        }
        gen_fail(p, GEN_ERR_PARSE, "expected path");
        return NULL;
    }

    switch (tok->kind) {
    case TOK_CINS:
        return gen_parse_type_decl(p, GEN_AST_GENUS);
    case TOK_TUR:
        return gen_parse_type_decl(p, GEN_AST_SPECIES);
    case TOK_KUME:
        return gen_parse_set_decl(p);
    case TOK_VERI:
        return gen_parse_data_decl(p);
    case TOK_UYE:
        return gen_parse_membership(p);
    case TOK_ICEAKTAR:
        return gen_parse_import(p);
    default:
        break;
    }
    if (p->mode == GEN_PARSE_DOCUMENT) {
        gen_fail(
            p,
            GEN_ERR_PARSE,
            "expected declaration, found %s",
            gen_token_kind_name(tok->kind)
        );
        return NULL;
    }
    if (gen_is_command_kw(tok->kind)) {
        return gen_parse_command(p);
    }
    if (tok->kind == TOK_IDENT) {
        return gen_parse_path_from_ident(p, tok);
    }
    gen_fail(p, GEN_ERR_PARSE, "expected declaration, command, or path");
    return NULL;
}

static bool gen_is_decl(const GenAst *node)
{
    return node->kind == GEN_AST_GENUS || node->kind == GEN_AST_SPECIES ||
           node->kind == GEN_AST_SET || node->kind == GEN_AST_DATA ||
           node->kind == GEN_AST_MEMBERSHIP || node->kind == GEN_AST_IMPORT;
}

GenResult gen_parse(
    GenContext *ctx,
    const char *source,
    size_t length,
    GenParseMode mode,
    const char *origin_path,
    GenAstProgram *out_program
)
{
    GenParser p;
    GenResult rc;
    GenAst *root;
    GenAst **items = NULL;
    size_t count = 0;
    size_t cap = 0;
    const char *saved_path;

    if (ctx == NULL || source == NULL || out_program == NULL) {
        if (ctx != NULL) {
            gen_context_set_error(ctx, GEN_ERR_INVALID_ARGUMENT, 0, 0, 0, "invalid argument");
        }
        return GEN_ERR_INVALID_ARGUMENT;
    }
    memset(out_program, 0, sizeof(*out_program));

    memset(&p, 0, sizeof(p));
    p.ctx = ctx;
    p.source = source;
    p.source_len = length;
    p.mode = mode;
    p.origin = origin_path;
    saved_path = ctx->source_path;
    ctx->source_path = origin_path;
    p.arena = gen_arena_create();
    if (p.arena == NULL) {
        ctx->source_path = saved_path;
        gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
        return GEN_ERR_OUT_OF_MEMORY;
    }

    rc = gen_lex_all(
        ctx,
        source,
        length,
        mode == GEN_PARSE_REPL ? GEN_LEX_REPL : GEN_LEX_DOCUMENT,
        &p.tokens,
        &p.token_count
    );
    if (rc != GEN_OK) {
        ctx->source_path = saved_path;
        gen_arena_destroy(p.arena);
        return rc;
    }

    root = gen_node(&p, GEN_AST_PROGRAM, gen_cur(&p));
    if (root == NULL) {
        ctx->source_path = saved_path;
        gen_tokens_free(p.tokens);
        gen_arena_destroy(p.arena);
        gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
        return GEN_ERR_OUT_OF_MEMORY;
    }

    while (!gen_check(&p, TOK_EOF)) {
        GenAst *stmt = gen_parse_statement(&p);
        GenAst **grown;
        if (stmt == NULL) {
            rc = ctx->last_error.code != GEN_OK ? ctx->last_error.code : GEN_ERR_PARSE;
            ctx->source_path = saved_path;
            gen_tokens_free(p.tokens);
            gen_arena_destroy(p.arena);
            return rc;
        }
        if (mode == GEN_PARSE_DOCUMENT && !gen_is_decl(stmt)) {
            ctx->source_path = saved_path;
            gen_tokens_free(p.tokens);
            gen_arena_destroy(p.arena);
            gen_context_set_error(
                ctx,
                GEN_ERR_PARSE,
                stmt->line,
                stmt->column,
                stmt->offset,
                "commands are not allowed in documents"
            );
            return GEN_ERR_PARSE;
        }
        if (mode == GEN_PARSE_PATH &&
            stmt->kind != GEN_AST_IDENT &&
            stmt->kind != GEN_AST_PROP_ACCESS &&
            stmt->kind != GEN_AST_INDEX_ACCESS) {
            ctx->source_path = saved_path;
            gen_tokens_free(p.tokens);
            gen_arena_destroy(p.arena);
            gen_context_set_error(
                ctx,
                GEN_ERR_PARSE,
                stmt->line,
                stmt->column,
                stmt->offset,
                "expected a path"
            );
            return GEN_ERR_PARSE;
        }
        if (count + 1u > cap) {
            cap = cap == 0 ? 8u : cap * 2u;
            grown = (GenAst **)gen_arena_alloc(p.arena, cap * sizeof(GenAst *));
            if (grown == NULL) {
                ctx->source_path = saved_path;
                gen_tokens_free(p.tokens);
                gen_arena_destroy(p.arena);
                gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
                return GEN_ERR_OUT_OF_MEMORY;
            }
            if (items != NULL && count > 0) {
                memcpy(grown, items, count * sizeof(GenAst *));
            }
            items = grown;
        }
        items[count++] = stmt;
    }

    root->u.program.items = items;
    root->u.program.count = count;
    gen_tokens_free(p.tokens);
    out_program->arena = p.arena;
    out_program->root = root;
    ctx->source_path = saved_path;
    return GEN_OK;
}
