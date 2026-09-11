"""cffi ABI loader for libgenlang."""

from __future__ import annotations

import os
from pathlib import Path

from cffi import FFI

ffi = FFI()
ffi.cdef(
    """
    typedef long long int64_t;

    typedef struct GenContext GenContext;
    typedef struct GenDocument GenDocument;
    typedef struct GenValue GenValue;
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

    const char *gen_version(void);
    int gen_version_major(void);
    int gen_version_minor(void);
    int gen_version_patch(void);

    GenContext *gen_context_create(void);
    void gen_context_free(GenContext *ctx);
    void gen_context_clear_error(GenContext *ctx);
    const GenError *gen_context_last_error(const GenContext *ctx);
    size_t gen_context_error_count(const GenContext *ctx);
    const GenError *gen_context_error(const GenContext *ctx, size_t index);

    GenResult gen_error_code(const GenError *error);
    const char *gen_error_message(const GenError *error);
    size_t gen_error_line(const GenError *error);
    size_t gen_error_column(const GenError *error);
    size_t gen_error_offset(const GenError *error);
    const char *gen_error_path(const GenError *error);

    GenResult gen_document_parse(GenContext *ctx, const char *source, GenDocument **out_document);
    GenResult gen_document_parse_at(GenContext *ctx, const char *source, const char *origin_path, GenDocument **out_document);
    GenResult gen_document_load_file(GenContext *ctx, const char *path, GenDocument **out_document);
    GenResult gen_document_serialize(const GenDocument *document, char **out_text, size_t *out_length);
    GenResult gen_document_to_json(const GenDocument *document, char **out_text, size_t *out_length);
    GenResult gen_document_from_json(GenContext *ctx, const char *json, GenDocument **out_document);
    GenResult gen_document_to_yaml(const GenDocument *document, char **out_text, size_t *out_length);
    GenResult gen_document_from_yaml(GenContext *ctx, const char *yaml, GenDocument **out_document);
    GenResult gen_document_to_binary(const GenDocument *document, char **out_bytes, size_t *out_length);
    GenResult gen_document_from_binary(GenContext *ctx, const char *data, size_t length, GenDocument **out_document);
    void gen_document_free(GenDocument *document);
    void gen_string_free(char *string);

    size_t gen_type_count(const GenDocument *document);
    GenResult gen_type_name(const GenDocument *document, size_t index, const char **out_name);
    GenResult gen_type_kind(const GenDocument *document, size_t index, GenTypeKind *out_kind);
    GenResult gen_type_parent(const GenDocument *document, const char *name, const char **out_parent);
    GenResult gen_type_location(const GenDocument *document, const char *name, size_t *out_line, size_t *out_column);

    size_t gen_set_count(const GenDocument *document);
    GenResult gen_set_name(const GenDocument *document, size_t index, const char **out_name);
    GenResult gen_set_location(const GenDocument *document, const char *name, size_t *out_line, size_t *out_column);

    size_t gen_entity_count(const GenDocument *document);
    GenResult gen_entity_name(const GenDocument *document, size_t index, const char **out_name);
    GenResult gen_entity_type_name(const GenDocument *document, const char *name, const char **out_type);
    GenResult gen_entity_location(const GenDocument *document, const char *name, size_t *out_line, size_t *out_column);
    GenResult gen_entity_value(const GenDocument *document, const char *name, const GenValue **out_value);

    GenValueType gen_value_type(const GenValue *value);
    int gen_value_bool(const GenValue *value);
    int64_t gen_value_int(const GenValue *value);
    double gen_value_float(const GenValue *value);
    const char *gen_value_string(const GenValue *value);
    const char *gen_value_reference(const GenValue *value);
    size_t gen_value_list_count(const GenValue *value);
    GenResult gen_value_list_get(const GenValue *value, size_t index, const GenValue **out_item);
    size_t gen_value_object_count(const GenValue *value);
    GenResult gen_value_object_key(const GenValue *value, size_t index, const char **out_key);
    GenResult gen_value_object_get(const GenValue *value, const char *key, const GenValue **out_item);
    GenResult gen_value_object_get_index(const GenValue *value, size_t index, const GenValue **out_item);
    void gen_value_free(GenValue *value);

    GenResult gen_get(const GenDocument *document, const char *path, GenValue **out_value);

    GenResult gen_types_of(const GenDocument *document, const char *name, GenQueryResult **out_result);
    GenResult gen_ancestors_of(const GenDocument *document, const char *name, GenQueryResult **out_result);
    GenResult gen_descendants_of(const GenDocument *document, const char *name, GenQueryResult **out_result);
    GenResult gen_memberships_of(const GenDocument *document, const char *name, GenQueryResult **out_result);
    GenResult gen_set_members(const GenDocument *document, const char *set_name, GenQueryResult **out_result);
    GenResult gen_entities_of(const GenDocument *document, const char *type_name, GenQueryResult **out_result);
    GenResult gen_is_member(const GenDocument *document, const char *set_name, const char *entity_name, int *out_is_member);
    size_t gen_query_result_count(const GenQueryResult *result);
    const char *gen_query_result_name(const GenQueryResult *result, size_t index);
    void gen_query_result_free(GenQueryResult *result);
    """
)


def _candidate_paths() -> list[Path]:
    env = os.environ.get("GENLANG_LIBRARY")
    if env:
        return [Path(env)]
    here = Path(__file__).resolve()
    root = here.parents[4]
    names = ("libgenlang.so", "libgenlang.dylib", "genlang.dll")
    dirs = [
        Path(os.environ["GENLANG_LIB_DIR"]) if "GENLANG_LIB_DIR" in os.environ else None,
        root / "build",
        root / "build-san",
    ]
    out: list[Path] = []
    for directory in dirs:
        if directory is None:
            continue
        for name in names:
            out.append(directory / name)
    return out


def _load():
    last_error = None
    for path in _candidate_paths():
        if path.is_file():
            try:
                return ffi.dlopen(str(path))
            except OSError as exc:
                last_error = exc
    try:
        return ffi.dlopen("genlang")
    except OSError as exc:
        last_error = exc
    raise OSError(
        "could not load libgenlang; build the C library and set GENLANG_LIBRARY "
        "or GENLANG_LIB_DIR"
    ) from last_error


lib = _load()
