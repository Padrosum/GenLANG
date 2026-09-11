#ifndef GENLANG_H
#define GENLANG_H

/*
 * GenLang public C API.
 *
 * This header is the stable ABI surface for libgenlang. All structures are
 * opaque. Bindings in other languages should target these functions only.
 *
 * Ownership conventions:
 *   OWNED    — caller must free with the matching gen_*_free function
 *   BORROWED — valid until the owning object (context, document, or value)
 *              is destroyed; do not free
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GENLANG_VERSION_MAJOR 0
#define GENLANG_VERSION_MINOR 1
#define GENLANG_VERSION_PATCH 0
#define GENLANG_VERSION_STRING "0.1.0"

#if defined(_WIN32) || defined(__CYGWIN__)
#  ifdef GENLANG_BUILD_SHARED
#    define GENLANG_API __declspec(dllexport)
#  elif defined(GENLANG_SHARED)
#    define GENLANG_API __declspec(dllimport)
#  else
#    define GENLANG_API
#  endif
#else
#  if defined(GENLANG_BUILD_SHARED) && defined(__GNUC__)
#    define GENLANG_API __attribute__((visibility("default")))
#  else
#    define GENLANG_API
#  endif
#endif

typedef struct GenContext GenContext;
typedef struct GenDocument GenDocument;
typedef struct GenValue GenValue;
typedef struct GenType GenType;
typedef struct GenSet GenSet;
typedef struct GenError GenError;
typedef struct GenQueryResult GenQueryResult;

typedef enum {
    GEN_OK = 0,
    GEN_ERR_INVALID_ARGUMENT,
    GEN_ERR_OUT_OF_MEMORY,
    GEN_ERR_IO,
    GEN_ERR_LEX,
    GEN_ERR_PARSE,
    GEN_ERR_SEMANTIC,
    GEN_ERR_NOT_FOUND,
    GEN_ERR_TYPE,
    GEN_ERR_INDEX,
    GEN_ERR_DUPLICATE,
    GEN_ERR_CYCLE,
    GEN_ERR_INVALID_OPERATION,
    GEN_ERR_SERIALIZATION,
    GEN_ERR_QUERY
} GenResult;

typedef enum {
    GEN_VALUE_NULL = 0,
    GEN_VALUE_BOOL,
    GEN_VALUE_INT,
    GEN_VALUE_FLOAT,
    GEN_VALUE_STRING,
    GEN_VALUE_LIST,
    GEN_VALUE_OBJECT,
    GEN_VALUE_REFERENCE
} GenValueType;

typedef enum {
    GEN_TYPE_KIND_GENUS = 0,
    GEN_TYPE_KIND_SPECIES
} GenTypeKind;

typedef struct {
    size_t max_source_size;
    size_t max_nesting_depth;
    size_t max_string_length;
    size_t max_object_properties;
    size_t max_list_length;
} GenLimits;

/* ---- Version ----------------------------------------------------------- */

GENLANG_API const char *gen_version(void);
GENLANG_API int gen_version_major(void);
GENLANG_API int gen_version_minor(void);
GENLANG_API int gen_version_patch(void);

/* ---- Context (OWNED) --------------------------------------------------- */

GENLANG_API GenContext *gen_context_create(void);
GENLANG_API void gen_context_free(GenContext *ctx);
GENLANG_API void gen_context_clear_error(GenContext *ctx);
GENLANG_API const GenError *gen_context_last_error(const GenContext *ctx);
GENLANG_API size_t gen_context_error_count(const GenContext *ctx);
GENLANG_API const GenError *gen_context_error(const GenContext *ctx, size_t index);
GENLANG_API GenLimits gen_context_limits(const GenContext *ctx);
GENLANG_API GenResult gen_context_set_limits(GenContext *ctx, const GenLimits *limits);

/* ---- Error (BORROWED from context) ------------------------------------ */

GENLANG_API GenResult gen_error_code(const GenError *error);
GENLANG_API const char *gen_error_message(const GenError *error);
GENLANG_API size_t gen_error_line(const GenError *error);
GENLANG_API size_t gen_error_column(const GenError *error);
GENLANG_API size_t gen_error_offset(const GenError *error);
/* BORROWED file path for this error, or NULL when the source was in-memory. */
GENLANG_API const char *gen_error_path(const GenError *error);

/* ---- Document ---------------------------------------------------------- */

/* Parse a NUL-terminated source buffer. *out_document is OWNED on success. */
GENLANG_API GenResult gen_document_parse(
    GenContext *ctx,
    const char *source,
    GenDocument **out_document
);

/*
 * Parse a buffer and resolve iceaktar paths relative to origin_path
 * (a file path, or NULL for in-memory sources that must not import).
 */
GENLANG_API GenResult gen_document_parse_at(
    GenContext *ctx,
    const char *source,
    const char *origin_path,
    GenDocument **out_document
);

/* Parse a source buffer of explicit length. *out_document is OWNED on success. */
GENLANG_API GenResult gen_document_parse_n(
    GenContext *ctx,
    const char *source,
    size_t length,
    GenDocument **out_document
);

/* Load and parse a file. *out_document is OWNED on success. */
GENLANG_API GenResult gen_document_load_file(
    GenContext *ctx,
    const char *path,
    GenDocument **out_document
);

/* Serialize document and write it to path. */
GENLANG_API GenResult gen_document_save_file(
    GenContext *ctx,
    const char *path,
    const GenDocument *document
);

/*
 * Serialize a document to canonical GenLang text.
 * *out_text is OWNED (free with gen_string_free).
 */
GENLANG_API GenResult gen_document_serialize(
    const GenDocument *document,
    char **out_text,
    size_t *out_length
);

/*
 * Serialize a document to JSON, preserving types, sets, values, and memberships.
 * *out_text is OWNED (free with gen_string_free).
 */
GENLANG_API GenResult gen_document_to_json(
    const GenDocument *document,
    char **out_text,
    size_t *out_length
);

/* Parse JSON produced by gen_document_to_json. *out_document is OWNED on success. */
GENLANG_API GenResult gen_document_from_json(
    GenContext *ctx,
    const char *json,
    GenDocument **out_document
);

/*
 * Serialize a document to YAML with the same schema as JSON
 * (types, sets, entities, memberships).
 * *out_text is OWNED (free with gen_string_free).
 */
GENLANG_API GenResult gen_document_to_yaml(
    const GenDocument *document,
    char **out_text,
    size_t *out_length
);

/* Parse YAML produced by gen_document_to_yaml (JSON is also accepted). */
GENLANG_API GenResult gen_document_from_yaml(
    GenContext *ctx,
    const char *yaml,
    GenDocument **out_document
);

/*
 * Serialize a document to the compact GenLang binary form (little-endian).
 * *out_bytes is OWNED (free with gen_string_free).
 */
GENLANG_API GenResult gen_document_to_binary(
    const GenDocument *document,
    char **out_bytes,
    size_t *out_length
);

/* Parse bytes produced by gen_document_to_binary. *out_document is OWNED on success. */
GENLANG_API GenResult gen_document_from_binary(
    GenContext *ctx,
    const char *data,
    size_t length,
    GenDocument **out_document
);

GENLANG_API void gen_document_free(GenDocument *document);

/* ---- Strings ----------------------------------------------------------- */

GENLANG_API void gen_string_free(char *string);

/* ---- Types / sets / entities (names BORROWED from document) ------------ */

GENLANG_API size_t gen_type_count(const GenDocument *document);
GENLANG_API GenResult gen_type_name(
    const GenDocument *document,
    size_t index,
    const char **out_name
);
GENLANG_API GenResult gen_type_kind(
    const GenDocument *document,
    size_t index,
    GenTypeKind *out_kind
);
GENLANG_API GenResult gen_type_parent(
    const GenDocument *document,
    const char *name,
    const char **out_parent
);
GENLANG_API GenResult gen_type_location(
    const GenDocument *document,
    const char *name,
    size_t *out_line,
    size_t *out_column
);

/* BORROWED object or NULL when the type has no properties. */
GENLANG_API GenResult gen_type_properties(
    const GenDocument *document,
    const char *name,
    const GenValue **out_value
);
GENLANG_API GenResult gen_type_property(
    const GenDocument *document,
    const char *type_name,
    const char *key,
    const GenValue **out_value
);

GENLANG_API size_t gen_set_count(const GenDocument *document);
GENLANG_API GenResult gen_set_name(
    const GenDocument *document,
    size_t index,
    const char **out_name
);
GENLANG_API GenResult gen_set_location(
    const GenDocument *document,
    const char *name,
    size_t *out_line,
    size_t *out_column
);
GENLANG_API GenResult gen_set_properties(
    const GenDocument *document,
    const char *name,
    const GenValue **out_value
);
GENLANG_API GenResult gen_set_property(
    const GenDocument *document,
    const char *set_name,
    const char *key,
    const GenValue **out_value
);

GENLANG_API size_t gen_entity_count(const GenDocument *document);
GENLANG_API GenResult gen_entity_name(
    const GenDocument *document,
    size_t index,
    const char **out_name
);
GENLANG_API GenResult gen_entity_type_name(
    const GenDocument *document,
    const char *name,
    const char **out_type
);
GENLANG_API GenResult gen_entity_location(
    const GenDocument *document,
    const char *name,
    size_t *out_line,
    size_t *out_column
);

/* Borrowed pointer into the document. Valid until the document is freed. */
GENLANG_API GenResult gen_entity_value(
    const GenDocument *document,
    const char *name,
    const GenValue **out_value
);

/* ---- Values ------------------------------------------------------------ */

GENLANG_API GenValueType gen_value_type(const GenValue *value);
GENLANG_API int gen_value_bool(const GenValue *value);
GENLANG_API int64_t gen_value_int(const GenValue *value);
GENLANG_API double gen_value_float(const GenValue *value);

/* BORROWED: owned by value, valid until value is destroyed. */
GENLANG_API const char *gen_value_string(const GenValue *value);
GENLANG_API size_t gen_value_string_length(const GenValue *value);
GENLANG_API const char *gen_value_reference(const GenValue *value);

GENLANG_API size_t gen_value_list_count(const GenValue *value);
GENLANG_API GenResult gen_value_list_get(
    const GenValue *value,
    size_t index,
    const GenValue **out_item
);

GENLANG_API size_t gen_value_object_count(const GenValue *value);
GENLANG_API GenResult gen_value_object_key(
    const GenValue *value,
    size_t index,
    const char **out_key
);
GENLANG_API GenResult gen_value_object_get(
    const GenValue *value,
    const char *key,
    const GenValue **out_item
);
GENLANG_API GenResult gen_value_object_get_index(
    const GenValue *value,
    size_t index,
    const GenValue **out_item
);

/* Deep clone. *out_value is OWNED. */
GENLANG_API GenResult gen_value_clone(
    const GenValue *value,
    GenValue **out_value
);

/*
 * Format a value as GenLang text.
 * *out_text is OWNED (free with gen_string_free).
 */
GENLANG_API GenResult gen_value_format(
    const GenValue *value,
    char **out_text,
    size_t *out_length
);

/*
 * Format a value as JSON.
 * References become {"$ref":"name"}.
 * *out_text is OWNED (free with gen_string_free).
 */
GENLANG_API GenResult gen_value_to_json(
    const GenValue *value,
    char **out_text,
    size_t *out_length
);

/*
 * Format a value as YAML.
 * References become a mapping with a `$ref` key.
 * *out_text is OWNED (free with gen_string_free).
 */
GENLANG_API GenResult gen_value_to_yaml(
    const GenValue *value,
    char **out_text,
    size_t *out_length
);

GENLANG_API void gen_value_free(GenValue *value);

/* ---- Path / get (OWNED value) ------------------------------------------ */

GENLANG_API GenResult gen_get(
    const GenDocument *document,
    const char *path,
    GenValue **out_value
);

GENLANG_API GenResult gen_eval_path(
    const GenDocument *document,
    const char *path,
    GenValue **out_value
);

/* ---- Queries (OWNED result) -------------------------------------------- */

GENLANG_API GenResult gen_types_of(
    const GenDocument *document,
    const char *name,
    GenQueryResult **out_result
);

GENLANG_API GenResult gen_ancestors_of(
    const GenDocument *document,
    const char *name,
    GenQueryResult **out_result
);

GENLANG_API GenResult gen_descendants_of(
    const GenDocument *document,
    const char *name,
    GenQueryResult **out_result
);

GENLANG_API GenResult gen_memberships_of(
    const GenDocument *document,
    const char *name,
    GenQueryResult **out_result
);

GENLANG_API GenResult gen_set_members(
    const GenDocument *document,
    const char *set_name,
    GenQueryResult **out_result
);

/*
 * Entities whose declared type is type_name or a subtype of it,
 * in declaration order. Unknown type is GEN_ERR_NOT_FOUND.
 */
GENLANG_API GenResult gen_entities_of(
    const GenDocument *document,
    const char *type_name,
    GenQueryResult **out_result
);

GENLANG_API GenResult gen_is_member(
    const GenDocument *document,
    const char *set_name,
    const char *entity_name,
    int *out_is_member
);

GENLANG_API GenResult gen_type_path(
    const GenDocument *document,
    const char *from_name,
    const char *to_name,
    GenQueryResult **out_result
);

GENLANG_API GenResult gen_search(
    const GenDocument *document,
    const char *query,
    GenQueryResult **out_result
);

GENLANG_API size_t gen_query_result_count(const GenQueryResult *result);
GENLANG_API const char *gen_query_result_name(
    const GenQueryResult *result,
    size_t index
);
GENLANG_API void gen_query_result_free(GenQueryResult *result);

/* Format helpers used by the CLI (OWNED strings). */
GENLANG_API GenResult gen_format_dyaz(
    const GenDocument *document,
    const char *name,
    char **out_text
);
GENLANG_API GenResult gen_format_goster(
    const GenDocument *document,
    const char *name,
    char **out_text
);

#ifdef __cplusplus
}
#endif

#endif /* GENLANG_H */
