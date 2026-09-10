#ifndef GENLANG_RUNTIME_H
#define GENLANG_RUNTIME_H

#include "internal/common.h"
#include "internal/hashmap.h"
#include "internal/string.h"
#include "internal/vector.h"
#include "memory/allocator.h"

typedef enum {
    GEN_REL_SUBTYPE_OF = 0,
    GEN_REL_MEMBER_OF,
    GEN_REL_TYPE_OF,
    GEN_REL_HAS_PROPERTY,
    GEN_REL_CONTAINS,
    GEN_REL_REFERENCES
} GenRelationType;

typedef struct GenProp {
    char *key;
    struct GenValue *value;
    UT_hash_handle hh;
} GenProp;

struct GenValue {
    GenValueType kind;
    union {
        bool boolean;
        int64_t integer;
        double floating;
        GenString string;
        struct {
            struct GenValue **items;
            size_t count;
        } list;
        struct {
            GenProp *by_name;
            char **keys;
            struct GenValue **values;
            size_t count;
        } object;
        GenString reference;
    } u;
};

typedef struct GenTypeNode GenTypeNode;
typedef struct GenSetNode GenSetNode;
typedef struct GenEntity GenEntity;

struct GenTypeNode {
    char *name;
    GenTypeKind kind;
    char *parent_name;
    GenTypeNode *parent;
    GenValue *props;
    GenPtrVec children;
    size_t decl_index;
    size_t line;
    size_t column;
    char *source_path;
    UT_hash_handle hh;
};

struct GenSetNode {
    char *name;
    GenValue *props;
    GenPtrVec members;
    size_t decl_index;
    size_t line;
    size_t column;
    char *source_path;
    UT_hash_handle hh;
};

struct GenEntity {
    char *name;
    char *type_name;
    GenTypeNode *type;
    GenValue *value;
    GenPtrVec sets;
    size_t decl_index;
    size_t line;
    size_t column;
    char *source_path;
    UT_hash_handle hh;
};

typedef struct {
    GenRelationType kind;
    char *from;
    char *to;
} GenRelationRec;

typedef struct {
    char *entity;
    char *set;
} GenMembershipRec;

struct GenDocument {
    GenTypeNode *types;
    GenSetNode *sets;
    GenEntity *entities;
    GenPtrVec types_order;
    GenPtrVec sets_order;
    GenPtrVec entities_order;
    GenPtrVec relations;
    GenPtrVec memberships;
};

struct GenQueryResult {
    char **names;
    size_t count;
};

struct GenType {
    int unused;
};

struct GenSet {
    int unused;
};

GenValue *gen_value_new(GenValueType kind);
void gen_value_destroy(GenValue *value);
GenResult gen_value_clone_impl(const GenValue *value, GenValue **out_value);
GenResult gen_value_format_impl(const GenValue *value, int indent, GenStrBuf *buf);
GenResult gen_value_object_put(GenValue *object, const char *key, GenValue *item);

GenTypeNode *gen_type_lookup(const GenDocument *doc, const char *name);
GenSetNode *gen_set_lookup(const GenDocument *doc, const char *name);
GenEntity *gen_entity_lookup(const GenDocument *doc, const char *name);

GenResult gen_type_collect_ancestors(const GenTypeNode *type, GenStrVec *out);
GenResult gen_type_collect_descendants(const GenTypeNode *type, GenStrVec *out);

GenDocument *gen_document_new(void);
void gen_document_destroy(GenDocument *doc);

GenResult gen_relation_add(
    GenDocument *doc,
    GenRelationType kind,
    const char *from,
    const char *to
);

#endif /* GENLANG_RUNTIME_H */
