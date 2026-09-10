#include "semantic/import.h"

#include "error/error.h"
#include "io/file.h"
#include "io/path.h"
#include "parser/parser.h"

typedef struct {
    GenPtrVec arenas;
    GenPtrVec items;
    GenStrVec stack;
    GenStrVec seen;
} GenImportState;

void gen_expanded_ast_free(GenExpandedAst *expanded)
{
    size_t i;

    if (expanded == NULL) {
        return;
    }
    for (i = 0; i < expanded->arenas.count; i++) {
        gen_arena_destroy((GenArena *)expanded->arenas.items[i]);
    }
    gen_ptrvec_free(&expanded->arenas);
    expanded->root = NULL;
}

static void gen_import_state_free(GenImportState *st)
{
    size_t i;
    if (st == NULL) {
        return;
    }
    for (i = 0; i < st->arenas.count; i++) {
        gen_arena_destroy((GenArena *)st->arenas.items[i]);
    }
    gen_ptrvec_free(&st->arenas);
    gen_ptrvec_free(&st->items);
    gen_strvec_free_all(&st->stack);
    gen_strvec_free_all(&st->seen);
}

static bool gen_path_looks_like_url(const char *path)
{
    return strstr(path, "://") != NULL;
}

static GenResult gen_expand_source(
    GenContext *ctx,
    GenImportState *st,
    const char *origin_path,
    const char *source,
    size_t length
);

static GenResult gen_expand_import(
    GenContext *ctx,
    GenImportState *st,
    const char *importer_path,
    const GenAst *stmt
)
{
    const char *spec = stmt->u.import.path;
    char *dir = NULL;
    char *joined = NULL;
    char *canonical = NULL;
    char *data = NULL;
    size_t length = 0;
    GenResult rc;
    const char *saved_path;

    saved_path = ctx->source_path;
    ctx->source_path = stmt->origin != NULL ? stmt->origin : importer_path;

    if (spec == NULL || spec[0] == '\0') {
        gen_context_set_error(
            ctx, GEN_ERR_SEMANTIC, stmt->line, stmt->column, stmt->offset, "iceaktar path is empty"
        );
        ctx->source_path = saved_path;
        return GEN_ERR_SEMANTIC;
    }
    if (gen_path_looks_like_url(spec)) {
        gen_context_set_error(
            ctx,
            GEN_ERR_SEMANTIC,
            stmt->line,
            stmt->column,
            stmt->offset,
            "iceaktar only accepts local .gl files"
        );
        ctx->source_path = saved_path;
        return GEN_ERR_SEMANTIC;
    }
    if (!gen_path_has_gl_suffix(spec)) {
        gen_context_set_error(
            ctx,
            GEN_ERR_SEMANTIC,
            stmt->line,
            stmt->column,
            stmt->offset,
            "iceaktar path must end with '.gl'"
        );
        ctx->source_path = saved_path;
        return GEN_ERR_SEMANTIC;
    }
    if (importer_path == NULL || importer_path[0] == '\0') {
        gen_context_set_error(
            ctx,
            GEN_ERR_IO,
            stmt->line,
            stmt->column,
            stmt->offset,
            "iceaktar requires a file origin; use gen_document_load_file or gen_document_parse_at"
        );
        ctx->source_path = saved_path;
        return GEN_ERR_IO;
    }

    dir = gen_path_dirname(importer_path);
    joined = gen_path_join(dir, spec);
    gen_free(dir);
    if (joined == NULL) {
        ctx->source_path = saved_path;
        gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
        return GEN_ERR_OUT_OF_MEMORY;
    }

    rc = gen_io_read_file(ctx, joined, &data, &length);
    if (rc != GEN_OK) {
        gen_free(joined);
        ctx->source_path = saved_path;
        return rc;
    }

    canonical = gen_path_canonical(joined);
    gen_free(joined);
    if (canonical == NULL) {
        gen_free(data);
        ctx->source_path = saved_path;
        gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
        return GEN_ERR_OUT_OF_MEMORY;
    }

    if (gen_strvec_contains(&st->stack, canonical)) {
        gen_context_set_error(
            ctx,
            GEN_ERR_CYCLE,
            stmt->line,
            stmt->column,
            stmt->offset,
            "import cycle involving '%s'",
            canonical
        );
        gen_free(canonical);
        gen_free(data);
        ctx->source_path = saved_path;
        return GEN_ERR_CYCLE;
    }
    if (gen_strvec_contains(&st->seen, canonical)) {
        gen_free(canonical);
        gen_free(data);
        ctx->source_path = saved_path;
        return GEN_OK;
    }

    if (st->stack.count >= ctx->limits.max_nesting_depth) {
        gen_context_set_error(
            ctx,
            GEN_ERR_SEMANTIC,
            stmt->line,
            stmt->column,
            stmt->offset,
            "import nesting exceeds maximum depth"
        );
        gen_free(canonical);
        gen_free(data);
        ctx->source_path = saved_path;
        return GEN_ERR_SEMANTIC;
    }

    if (!gen_strvec_push(&st->stack, canonical)) {
        gen_free(canonical);
        gen_free(data);
        ctx->source_path = saved_path;
        gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
        return GEN_ERR_OUT_OF_MEMORY;
    }

    rc = gen_expand_source(ctx, st, st->stack.items[st->stack.count - 1u], data, length);
    gen_free(data);
    if (rc != GEN_OK) {
        ctx->source_path = saved_path;
        return rc;
    }

    canonical = st->stack.items[st->stack.count - 1u];
    st->stack.items[st->stack.count - 1u] = NULL;
    st->stack.count--;
    if (!gen_strvec_push(&st->seen, canonical)) {
        gen_free(canonical);
        ctx->source_path = saved_path;
        gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
        return GEN_ERR_OUT_OF_MEMORY;
    }
    ctx->source_path = saved_path;
    return GEN_OK;
}

static GenResult gen_expand_source(
    GenContext *ctx,
    GenImportState *st,
    const char *origin_path,
    const char *source,
    size_t length
)
{
    GenAstProgram program;
    GenResult rc;
    size_t i;

    if (length > ctx->limits.max_source_size) {
        gen_context_set_error(
            ctx,
            GEN_ERR_LEX,
            0,
            0,
            0,
            "source exceeds maximum size of %zu bytes",
            ctx->limits.max_source_size
        );
        return GEN_ERR_LEX;
    }

    rc = gen_parse(ctx, source, length, GEN_PARSE_DOCUMENT, origin_path, &program);
    if (rc != GEN_OK) {
        return rc;
    }
    if (!gen_ptrvec_push(&st->arenas, program.arena)) {
        gen_ast_program_free(&program);
        gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
        return GEN_ERR_OUT_OF_MEMORY;
    }
    program.arena = NULL;

    for (i = 0; i < program.root->u.program.count; i++) {
        GenAst *stmt = program.root->u.program.items[i];
        if (stmt->kind == GEN_AST_IMPORT) {
            rc = gen_expand_import(ctx, st, origin_path, stmt);
            if (rc != GEN_OK) {
                return rc;
            }
        } else if (!gen_ptrvec_push(&st->items, stmt)) {
            gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
            return GEN_ERR_OUT_OF_MEMORY;
        }
    }
    return GEN_OK;
}

GenResult gen_expand_document(
    GenContext *ctx,
    const char *source,
    size_t length,
    const char *origin_path,
    GenExpandedAst *out_expanded
)
{
    GenImportState st;
    GenArena *holder;
    GenAst *root;
    GenAst **copy;
    size_t i;
    GenResult rc;
    char *root_id = NULL;

    if (ctx == NULL || source == NULL || out_expanded == NULL) {
        if (ctx != NULL) {
            gen_context_set_error(ctx, GEN_ERR_INVALID_ARGUMENT, 0, 0, 0, "invalid argument");
        }
        return GEN_ERR_INVALID_ARGUMENT;
    }

    memset(out_expanded, 0, sizeof(*out_expanded));
    memset(&st, 0, sizeof(st));
    gen_ptrvec_init(&st.arenas);
    gen_ptrvec_init(&st.items);
    gen_strvec_init(&st.stack);
    gen_strvec_init(&st.seen);

    if (origin_path != NULL && origin_path[0] != '\0') {
        root_id = gen_path_canonical(origin_path);
        if (root_id == NULL) {
            gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
            return GEN_ERR_OUT_OF_MEMORY;
        }
        if (!gen_strvec_push(&st.stack, root_id)) {
            gen_free(root_id);
            gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
            return GEN_ERR_OUT_OF_MEMORY;
        }
        root_id = NULL;
    }

    rc = gen_expand_source(ctx, &st, origin_path, source, length);
    if (rc != GEN_OK) {
        gen_import_state_free(&st);
        return rc;
    }

    holder = gen_arena_create();
    if (holder == NULL) {
        gen_import_state_free(&st);
        gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
        return GEN_ERR_OUT_OF_MEMORY;
    }
    root = gen_ast_new(holder, GEN_AST_PROGRAM, NULL);
    if (root == NULL) {
        gen_arena_destroy(holder);
        gen_import_state_free(&st);
        gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
        return GEN_ERR_OUT_OF_MEMORY;
    }
    if (st.items.count > 0) {
        copy = (GenAst **)gen_arena_alloc(holder, st.items.count * sizeof(GenAst *));
        if (copy == NULL) {
            gen_arena_destroy(holder);
            gen_import_state_free(&st);
            gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
            return GEN_ERR_OUT_OF_MEMORY;
        }
        for (i = 0; i < st.items.count; i++) {
            copy[i] = (GenAst *)st.items.items[i];
        }
        root->u.program.items = copy;
        root->u.program.count = st.items.count;
    }

    if (!gen_ptrvec_push(&st.arenas, holder)) {
        gen_arena_destroy(holder);
        gen_import_state_free(&st);
        gen_context_set_error(ctx, GEN_ERR_OUT_OF_MEMORY, 0, 0, 0, "out of memory");
        return GEN_ERR_OUT_OF_MEMORY;
    }

    out_expanded->arenas = st.arenas;
    out_expanded->root = root;
    gen_ptrvec_init(&st.arenas);
    gen_ptrvec_free(&st.items);
    gen_strvec_free_all(&st.stack);
    gen_strvec_free_all(&st.seen);
    return GEN_OK;
}
