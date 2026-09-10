#ifndef GENLANG_AST_H
#define GENLANG_AST_H

#include "internal/common.h"
#include "lexer/lexer.h"
#include "memory/allocator.h"

typedef enum {
    GEN_AST_PROGRAM = 0,
    GEN_AST_GENUS,
    GEN_AST_SPECIES,
    GEN_AST_SET,
    GEN_AST_DATA,
    GEN_AST_MEMBERSHIP,
    GEN_AST_IMPORT,
    GEN_AST_IDENT,
    GEN_AST_STRING,
    GEN_AST_INT,
    GEN_AST_FLOAT,
    GEN_AST_BOOL,
    GEN_AST_NULL,
    GEN_AST_LIST,
    GEN_AST_OBJECT,
    GEN_AST_PROP_ACCESS,
    GEN_AST_INDEX_ACCESS,
    GEN_AST_REFERENCE,
    GEN_AST_COMMAND,
    GEN_AST_PATH
} GenAstKind;

typedef enum {
    GEN_CMD_DYAZ = 0,
    GEN_CMD_GOSTER,
    GEN_CMD_UYELER,
    GEN_CMD_ICERIR,
    GEN_CMD_USTLER,
    GEN_CMD_ALTLAR,
    GEN_CMD_YOL,
    GEN_CMD_ARA,
    GEN_CMD_LISTE,
    GEN_CMD_YARDIM,
    GEN_CMD_TEMIZLE,
    GEN_CMD_CIKIS
} GenCmdKind;

typedef struct GenAst GenAst;

struct GenAst {
    GenAstKind kind;
    const char *origin;
    size_t line;
    size_t column;
    size_t offset;
    union {
        struct {
            GenAst **items;
            size_t count;
        } program;
        struct {
            char *name;
            char *parent;
            GenAst *props;
        } type_decl;
        struct {
            char *name;
            GenAst *props;
        } set_decl;
        struct {
            char *name;
            char *type;
            GenAst *value;
        } data_decl;
        struct {
            char *entity;
            char *set;
        } membership;
        struct {
            char *path;
            size_t path_length;
        } import;
        struct {
            char *name;
        } ident;
        struct {
            char *data;
            size_t length;
        } string;
        int64_t integer;
        double floating;
        bool boolean;
        struct {
            GenAst **items;
            size_t count;
        } list;
        struct {
            char **keys;
            GenAst **values;
            size_t count;
        } object;
        struct {
            GenAst *base;
            char *property;
        } prop_access;
        struct {
            GenAst *base;
            int64_t index;
            size_t index_line;
            size_t index_column;
        } index_access;
        struct {
            char *name;
        } reference;
        struct {
            GenCmdKind cmd;
            char *arg1;
            char *arg2;
        } command;
    } u;
};

typedef struct {
    GenArena *arena;
    GenAst *root;
} GenAstProgram;

GenAst *gen_ast_new(GenArena *arena, GenAstKind kind, const GenToken *tok);
void gen_ast_program_free(GenAstProgram *program);

#endif /* GENLANG_AST_H */
