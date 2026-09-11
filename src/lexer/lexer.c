#include "lexer/lexer.h"

#include "error/error.h"
#include "internal/string.h"
#include "memory/allocator.h"

static const GenKeyword GEN_DOC_KEYWORDS[] = {
    {"cins", TOK_CINS},
    {"tur", TOK_TUR},
    {"kume", TOK_KUME},
    {"veri", TOK_VERI},
    {"uye", TOK_UYE},
    {"iceaktar", TOK_ICEAKTAR},
    {"true", TOK_TRUE},
    {"false", TOK_FALSE},
    {"null", TOK_NULL}
};

static const GenKeyword GEN_REPL_KEYWORDS[] = {
    {"dyaz", TOK_DYAZ},
    {"goster", TOK_GOSTER},
    {"uyeler", TOK_UYELER},
    {"icerir", TOK_ICERIR},
    {"ustler", TOK_USTLER},
    {"altlar", TOK_ALTLAR},
    {"yol", TOK_YOL},
    {"ara", TOK_ARA},
    {"liste", TOK_LISTE},
    {"yardim", TOK_YARDIM},
    {"temizle", TOK_TEMIZLE},
    {"cikis", TOK_CIKIS}
};

static const GenKeyword GEN_KEYWORDS[] = {
    {"cins", TOK_CINS},
    {"tur", TOK_TUR},
    {"kume", TOK_KUME},
    {"veri", TOK_VERI},
    {"uye", TOK_UYE},
    {"iceaktar", TOK_ICEAKTAR},
    {"dyaz", TOK_DYAZ},
    {"goster", TOK_GOSTER},
    {"uyeler", TOK_UYELER},
    {"icerir", TOK_ICERIR},
    {"ustler", TOK_USTLER},
    {"altlar", TOK_ALTLAR},
    {"yol", TOK_YOL},
    {"ara", TOK_ARA},
    {"liste", TOK_LISTE},
    {"yardim", TOK_YARDIM},
    {"temizle", TOK_TEMIZLE},
    {"cikis", TOK_CIKIS},
    {"true", TOK_TRUE},
    {"false", TOK_FALSE},
    {"null", TOK_NULL}
};

const GenKeyword *gen_keywords(size_t *out_count)
{
    if (out_count != NULL) {
        *out_count = sizeof(GEN_KEYWORDS) / sizeof(GEN_KEYWORDS[0]);
    }
    return GEN_KEYWORDS;
}

const char *gen_token_kind_name(GenTokenKind kind)
{
    switch (kind) {
    case TOK_EOF:
        return "end of file";
    case TOK_IDENT:
        return "identifier";
    case TOK_STRING:
        return "string";
    case TOK_INT:
        return "integer";
    case TOK_FLOAT:
        return "float";
    case TOK_TRUE:
        return "true";
    case TOK_FALSE:
        return "false";
    case TOK_NULL:
        return "null";
    case TOK_CINS:
        return "cins";
    case TOK_TUR:
        return "tur";
    case TOK_KUME:
        return "kume";
    case TOK_VERI:
        return "veri";
    case TOK_UYE:
        return "uye";
    case TOK_ICEAKTAR:
        return "iceaktar";
    case TOK_DYAZ:
        return "dyaz";
    case TOK_GOSTER:
        return "goster";
    case TOK_UYELER:
        return "uyeler";
    case TOK_ICERIR:
        return "icerir";
    case TOK_USTLER:
        return "ustler";
    case TOK_ALTLAR:
        return "altlar";
    case TOK_YOL:
        return "yol";
    case TOK_ARA:
        return "ara";
    case TOK_LISTE:
        return "liste";
    case TOK_YARDIM:
        return "yardim";
    case TOK_TEMIZLE:
        return "temizle";
    case TOK_CIKIS:
        return "cikis";
    case TOK_LBRACE:
        return "'{'";
    case TOK_RBRACE:
        return "'}'";
    case TOK_LBRACKET:
        return "'['";
    case TOK_RBRACKET:
        return "']'";
    case TOK_COMMA:
        return "','";
    case TOK_DOT:
        return "'.'";
    case TOK_COLON:
        return "':'";
    case TOK_EQ:
        return "'='";
    case TOK_ARROW:
        return "'->'";
    case TOK_AT:
        return "'@'";
    default:
        return "token";
    }
}

bool gen_token_is_keyword(GenTokenKind kind)
{
    return kind >= TOK_TRUE && kind <= TOK_CIKIS;
}

static bool gen_is_space(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static GenTokenKind gen_lookup_in_table(
    const GenKeyword *table,
    size_t count,
    const char *s,
    size_t n
)
{
    size_t i;
    for (i = 0; i < count; i++) {
        if (strlen(table[i].text) == n && memcmp(table[i].text, s, n) == 0) {
            return table[i].kind;
        }
    }
    return TOK_IDENT;
}

static GenTokenKind gen_lookup_keyword(const char *s, size_t n, GenLexMode mode)
{
    GenTokenKind kind = gen_lookup_in_table(
        GEN_DOC_KEYWORDS, sizeof(GEN_DOC_KEYWORDS) / sizeof(GEN_DOC_KEYWORDS[0]), s, n
    );
    if (kind != TOK_IDENT) {
        return kind;
    }
    if (mode == GEN_LEX_REPL) {
        return gen_lookup_in_table(
            GEN_REPL_KEYWORDS,
            sizeof(GEN_REPL_KEYWORDS) / sizeof(GEN_REPL_KEYWORDS[0]),
            s,
            n
        );
    }
    return TOK_IDENT;
}

typedef struct {
    GenContext *ctx;
    const char *src;
    size_t length;
    size_t pos;
    size_t line;
    size_t column;
    GenToken *tokens;
    size_t count;
    size_t capacity;
} GenLexState;

static bool gen_lex_push(GenLexState *st, GenToken token)
{
    GenToken *grown;
    size_t cap;

    if (st->count + 1u > st->capacity) {
        cap = st->capacity == 0 ? 32u : st->capacity * 2u;
        grown = (GenToken *)gen_realloc(st->tokens, cap * sizeof(GenToken));
        if (grown == NULL) {
            gen_context_set_error(
                st->ctx, GEN_ERR_OUT_OF_MEMORY, st->line, st->column, st->pos, "out of memory"
            );
            return false;
        }
        st->tokens = grown;
        st->capacity = cap;
    }
    st->tokens[st->count++] = token;
    return true;
}

static unsigned char gen_lex_at(const GenLexState *st, size_t i)
{
    if (i >= st->length) {
        return 0;
    }
    return (unsigned char)st->src[i];
}

static void gen_lex_advance(GenLexState *st)
{
    if (st->pos >= st->length) {
        return;
    }
    if (st->src[st->pos] == '\n') {
        st->line++;
        st->column = 1;
    } else {
        st->column++;
    }
    st->pos++;
}

static void gen_lex_advance_bytes(GenLexState *st, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
        gen_lex_advance(st);
    }
    if (n > 1u) {
        st->column -= n - 1u;
    }
}

static bool gen_lex_string(GenLexState *st)
{
    GenToken tok;
    size_t start_line = st->line;
    size_t start_col = st->column;
    size_t start_off = st->pos;
    size_t decoded_len = 0;

    gen_lex_advance(st); /* opening quote */
    while (st->pos < st->length) {
        unsigned char c = gen_lex_at(st, st->pos);
        if (c == '"') {
            break;
        }
        if (c == '\n') {
            gen_context_set_error(
                st->ctx,
                GEN_ERR_LEX,
                start_line,
                start_col,
                start_off,
                "unterminated string"
            );
            return false;
        }
        if (c == '\\') {
            gen_lex_advance(st);
            if (st->pos >= st->length) {
                gen_context_set_error(
                    st->ctx,
                    GEN_ERR_LEX,
                    start_line,
                    start_col,
                    start_off,
                    "unterminated string escape"
                );
                return false;
            }
            c = gen_lex_at(st, st->pos);
            if (c != '"' && c != '\\' && c != 'n' && c != 't' && c != 'r') {
                gen_context_set_error(
                    st->ctx,
                    GEN_ERR_LEX,
                    st->line,
                    st->column,
                    st->pos,
                    "invalid escape sequence"
                );
                return false;
            }
        }
        decoded_len++;
        gen_lex_advance(st);
    }
    if (st->pos >= st->length || gen_lex_at(st, st->pos) != '"') {
        gen_context_set_error(
            st->ctx, GEN_ERR_LEX, start_line, start_col, start_off, "unterminated string"
        );
        return false;
    }
    if (decoded_len > st->ctx->limits.max_string_length) {
        gen_context_set_error(
            st->ctx,
            GEN_ERR_LEX,
            start_line,
            start_col,
            start_off,
            "string exceeds maximum length"
        );
        return false;
    }
    gen_lex_advance(st); /* closing quote */
    memset(&tok, 0, sizeof(tok));
    tok.kind = TOK_STRING;
    tok.lexeme = st->src + start_off;
    tok.length = st->pos - start_off;
    tok.line = start_line;
    tok.column = start_col;
    tok.offset = start_off;
    return gen_lex_push(st, tok);
}

static bool gen_lex_number(GenLexState *st)
{
    GenToken tok;
    size_t start_line = st->line;
    size_t start_col = st->column;
    size_t start_off = st->pos;
    bool is_float = false;
    char *tmp;
    char *end = NULL;

    if (gen_lex_at(st, st->pos) == '-') {
        gen_lex_advance(st);
    }
    if (st->pos >= st->length ||
        gen_lex_at(st, st->pos) < '0' ||
        gen_lex_at(st, st->pos) > '9') {
        gen_context_set_error(
            st->ctx, GEN_ERR_LEX, start_line, start_col, start_off, "invalid number"
        );
        return false;
    }
    while (st->pos < st->length &&
           gen_lex_at(st, st->pos) >= '0' &&
           gen_lex_at(st, st->pos) <= '9') {
        gen_lex_advance(st);
    }
    if (st->pos < st->length && gen_lex_at(st, st->pos) == '.') {
        size_t next = st->pos + 1u;
        if (next < st->length &&
            gen_lex_at(st, next) >= '0' &&
            gen_lex_at(st, next) <= '9') {
            is_float = true;
            gen_lex_advance(st);
            while (st->pos < st->length &&
                   gen_lex_at(st, st->pos) >= '0' &&
                   gen_lex_at(st, st->pos) <= '9') {
                gen_lex_advance(st);
            }
        }
    }
    if (st->pos < st->length &&
        (gen_lex_at(st, st->pos) == 'e' || gen_lex_at(st, st->pos) == 'E')) {
        size_t look = st->pos + 1u;
        is_float = true;
        gen_lex_advance(st);
        if (st->pos < st->length &&
            (gen_lex_at(st, st->pos) == '+' || gen_lex_at(st, st->pos) == '-')) {
            gen_lex_advance(st);
        }
        if (st->pos >= st->length ||
            gen_lex_at(st, st->pos) < '0' ||
            gen_lex_at(st, st->pos) > '9') {
            GEN_UNUSED(look);
            gen_context_set_error(
                st->ctx,
                GEN_ERR_LEX,
                start_line,
                start_col,
                start_off,
                "invalid floating-point exponent"
            );
            return false;
        }
        while (st->pos < st->length &&
               gen_lex_at(st, st->pos) >= '0' &&
               gen_lex_at(st, st->pos) <= '9') {
            gen_lex_advance(st);
        }
    }

    tmp = gen_strndup(st->src + start_off, st->pos - start_off);
    if (tmp == NULL) {
        gen_context_set_error(
            st->ctx, GEN_ERR_OUT_OF_MEMORY, start_line, start_col, start_off, "out of memory"
        );
        return false;
    }
    memset(&tok, 0, sizeof(tok));
    tok.lexeme = st->src + start_off;
    tok.length = st->pos - start_off;
    tok.line = start_line;
    tok.column = start_col;
    tok.offset = start_off;
    errno = 0;
    if (is_float) {
        tok.kind = TOK_FLOAT;
        tok.float_value = strtod(tmp, &end);
        if (errno == ERANGE || end == tmp) {
            gen_free(tmp);
            gen_context_set_error(
                st->ctx,
                GEN_ERR_LEX,
                start_line,
                start_col,
                start_off,
                "invalid floating-point number"
            );
            return false;
        }
    } else {
        tok.kind = TOK_INT;
        tok.int_value = strtoll(tmp, &end, 10);
        if (errno == ERANGE || end == tmp) {
            gen_free(tmp);
            gen_context_set_error(
                st->ctx, GEN_ERR_LEX, start_line, start_col, start_off, "integer out of range"
            );
            return false;
        }
    }
    gen_free(tmp);
    return gen_lex_push(st, tok);
}

GenResult gen_lex_all(
    GenContext *ctx,
    const char *src,
    size_t length,
    GenLexMode mode,
    GenToken **out_tokens,
    size_t *out_count
)
{
    GenLexState st;
    GenToken eof;

    if (ctx == NULL || src == NULL || out_tokens == NULL || out_count == NULL) {
        if (ctx != NULL) {
            gen_context_set_error(
                ctx, GEN_ERR_INVALID_ARGUMENT, 0, 0, 0, "invalid argument to lexer"
            );
        }
        return GEN_ERR_INVALID_ARGUMENT;
    }

    memset(&st, 0, sizeof(st));
    st.ctx = ctx;
    st.src = src;
    st.length = length;
    st.line = 1;
    st.column = 1;

    while (st.pos < st.length) {
        unsigned char c = gen_lex_at(&st, st.pos);
        size_t line = st.line;
        size_t col = st.column;
        size_t off = st.pos;
        GenToken tok;

        if (gen_is_space(c)) {
            gen_lex_advance(&st);
            continue;
        }
        if (c == '#') {
            while (st.pos < st.length && gen_lex_at(&st, st.pos) != '\n') {
                gen_lex_advance(&st);
            }
            continue;
        }

        memset(&tok, 0, sizeof(tok));
        tok.line = line;
        tok.column = col;
        tok.offset = off;
        tok.lexeme = src + off;

        if (c == '"') {
            if (!gen_lex_string(&st)) {
                gen_tokens_free(st.tokens);
                return ctx->last_error.code;
            }
            continue;
        }
        if (c == '-' &&
            st.pos + 1u < st.length &&
            gen_lex_at(&st, st.pos + 1u) == '>') {
            tok.kind = TOK_ARROW;
            tok.length = 2;
            gen_lex_advance(&st);
            gen_lex_advance(&st);
            if (!gen_lex_push(&st, tok)) {
                gen_tokens_free(st.tokens);
                return ctx->last_error.code;
            }
            continue;
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            if (!gen_lex_number(&st)) {
                gen_tokens_free(st.tokens);
                return ctx->last_error.code;
            }
            continue;
        }
        {
            uint32_t cp = 0;
            size_t n = 0;
            if (!gen_utf8_next(src, st.length, st.pos, &cp, &n)) {
                gen_context_set_error(
                    ctx, GEN_ERR_LEX, line, col, off, "invalid UTF-8 in source"
                );
                gen_tokens_free(st.tokens);
                return GEN_ERR_LEX;
            }
            if (gen_utf8_is_ident_start(cp)) {
                gen_lex_advance_bytes(&st, n);
                while (st.pos < st.length) {
                    if (!gen_utf8_next(src, st.length, st.pos, &cp, &n)) {
                        gen_context_set_error(
                            ctx, GEN_ERR_LEX, st.line, st.column, st.pos, "invalid UTF-8 in identifier"
                        );
                        gen_tokens_free(st.tokens);
                        return GEN_ERR_LEX;
                    }
                    if (!gen_utf8_is_ident_continue(cp)) {
                        break;
                    }
                    gen_lex_advance_bytes(&st, n);
                }
                tok.length = st.pos - off;
                tok.kind = gen_lookup_keyword(src + off, tok.length, mode);
                if (!gen_lex_push(&st, tok)) {
                    gen_tokens_free(st.tokens);
                    return ctx->last_error.code;
                }
                continue;
            }
        }

        switch (c) {
        case '{':
            tok.kind = TOK_LBRACE;
            break;
        case '}':
            tok.kind = TOK_RBRACE;
            break;
        case '[':
            tok.kind = TOK_LBRACKET;
            break;
        case ']':
            tok.kind = TOK_RBRACKET;
            break;
        case ',':
            tok.kind = TOK_COMMA;
            break;
        case '.':
            tok.kind = TOK_DOT;
            break;
        case ':':
            tok.kind = TOK_COLON;
            break;
        case '=':
            tok.kind = TOK_EQ;
            break;
        case '@':
            tok.kind = TOK_AT;
            break;
        default:
            gen_tokens_free(st.tokens);
            gen_context_set_error(
                ctx,
                GEN_ERR_LEX,
                line,
                col,
                off,
                "unexpected character U+%04X",
                (unsigned int)c
            );
            return GEN_ERR_LEX;
        }
        tok.length = 1;
        gen_lex_advance(&st);
        if (!gen_lex_push(&st, tok)) {
            gen_tokens_free(st.tokens);
            return ctx->last_error.code;
        }
    }

    memset(&eof, 0, sizeof(eof));
    eof.kind = TOK_EOF;
    eof.lexeme = src + st.pos;
    eof.line = st.line;
    eof.column = st.column;
    eof.offset = st.pos;
    if (!gen_lex_push(&st, eof)) {
        gen_tokens_free(st.tokens);
        return ctx->last_error.code;
    }

    *out_tokens = st.tokens;
    *out_count = st.count;
    return GEN_OK;
}

void gen_tokens_free(GenToken *tokens)
{
    gen_free(tokens);
}
