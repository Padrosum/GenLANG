# GenLang C API

All declarations live in `include/genlang.h`. Structures are opaque. Link `libgenlang`.

Learning path: [The GenLang Book](book/genlang-book.md), chapters 11–16. This page is the function-by-function reference.

**Thread safety:** different `GenContext` objects may be used independently. Do not mutate the same `GenDocument` from multiple threads.

**Ownership:**

- **OWNED** — caller frees with the matching `gen_*_free` / `gen_string_free`
- **BORROWED** — valid until the owning context, document, or value is destroyed

## Version

| Symbol | Notes |
| --- | --- |
| `GENLANG_VERSION_MAJOR/MINOR/PATCH` | `0`, `1`, `0` |
| `gen_version()` | BORROWED static string `"0.1.0"` |
| `gen_version_major/minor/patch()` | integer components |

## `GenResult`

```c
GEN_OK,
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
```

## Context

### `GenContext *gen_context_create(void)`

Creates an independent context (limits + last error). **OWNED.** Returns `NULL` on allocation failure.

### `void gen_context_free(GenContext *ctx)`

Frees a context. Accepts `NULL`.

### `void gen_context_clear_error(GenContext *ctx)`

Clears the last error to `GEN_OK`.

### `const GenError *gen_context_last_error(const GenContext *ctx)`

**BORROWED** pointer to the first recorded error slot. Valid until the context is freed or errors are cleared. Later errors do not overwrite this slot.

### `size_t gen_context_error_count(const GenContext *ctx)`

Number of recorded errors. Lexer and parser still stop at the first failure; the semantic analyzer may report several.

### `const GenError *gen_context_error(const GenContext *ctx, size_t index)`

**BORROWED** error at `index` (`0` .. `count - 1`). Returns `NULL` if the index is out of range.

### `GenLimits gen_context_limits(const GenContext *ctx)`

Returns a copy of the resource limits.

### `GenResult gen_context_set_limits(GenContext *ctx, const GenLimits *limits)`

Replaces limits. All fields must be greater than zero. Errors: `GEN_ERR_INVALID_ARGUMENT`.

## Error accessors

`gen_error_code`, `gen_error_message` (BORROWED), `gen_error_line`, `gen_error_column`, `gen_error_offset`, `gen_error_path` (BORROWED file path, or `NULL` for in-memory sources).

A `NULL` error yields `GEN_ERR_INVALID_ARGUMENT` / empty string / zero.

Example:

```c
if (result != GEN_OK) {
    const GenError *e = gen_context_last_error(ctx);
    fprintf(stderr, "%s\n", gen_error_message(e));
}
```

## Document

### `gen_document_parse(ctx, source, &doc)`

Parse a NUL-terminated buffer. `*out_document` is **OWNED** on `GEN_OK`.

Possible errors: `GEN_ERR_INVALID_ARGUMENT`, `GEN_ERR_OUT_OF_MEMORY`, `GEN_ERR_LEX`, `GEN_ERR_PARSE`, `GEN_ERR_SEMANTIC`, `GEN_ERR_DUPLICATE`, `GEN_ERR_CYCLE`.

`iceaktar` in an in-memory buffer (no origin path) returns `GEN_ERR_IO`.

### `gen_document_parse_at(ctx, source, origin_path, &doc)`

Same as `gen_document_parse`, but `iceaktar` paths are resolved relative to `origin_path` (normally a file path). `gen_document_load_file` uses this internally.

### `gen_document_parse_n(ctx, source, length, &doc)`

Same, with an explicit length (no requirement that `source` is NUL-terminated beyond `length` bytes).

### `gen_document_load_file(ctx, path, &doc)`

Reads a file then parses. I/O failures return `GEN_ERR_IO`.

### `gen_document_save_file(ctx, path, document)`

Serializes then writes. Errors: `GEN_ERR_IO`, `GEN_ERR_SERIALIZATION`, `GEN_ERR_INVALID_ARGUMENT`.

### `gen_document_serialize(document, &text, &length)`

Canonical GenLang text. `*out_text` is **OWNED** (`gen_string_free`). Output is deterministic.

### `gen_document_to_json` / `gen_document_from_json`

JSON object `{types, sets, entities, memberships}`. References are `{"$ref":"name"}`. `*out_text` is **OWNED**.

### `gen_document_to_yaml` / `gen_document_from_yaml`

YAML with the same schema. `from_yaml` also accepts JSON (YAML 1.2). Block scalars (`|`, `>`) are not supported. `*out_text` is **OWNED**.

### `gen_document_to_binary` / `gen_document_from_binary`

Compact little-endian binary with magic `GLB` + version `1`, then the same document schema. `from_binary` takes an explicit length (embedded NUL is allowed). `*out_bytes` is **OWNED** (`gen_string_free`).

### `void gen_document_free(GenDocument *document)`

### `void gen_string_free(char *string)`

## Listing (names BORROWED from the document)

- `gen_type_count` / `gen_type_name` / `gen_type_kind` / `gen_type_parent` / `gen_type_location`
- `gen_type_properties` / `gen_type_property` — **BORROWED** object or property; missing type or key is `GEN_ERR_NOT_FOUND`; a type with no properties returns `GEN_OK` and `NULL`
- `gen_set_count` / `gen_set_name` / `gen_set_location`
- `gen_set_properties` / `gen_set_property` — same conventions as type properties
- `gen_entity_count` / `gen_entity_name` / `gen_entity_type_name` / `gen_entity_location`
- `gen_entity_value` — **BORROWED** `const GenValue *` into the document

`gen_type_parent` writes `NULL` when the type has no parent. Location functions write 1-based line and column.

Out-of-range indexes return `GEN_ERR_INDEX`. Missing names return `GEN_ERR_NOT_FOUND`.

## Values

`gen_value_type` returns the tag. Accessors:

| Function | Meaning |
| --- | --- |
| `gen_value_bool` | `1`/`0`; `0` if not a bool |
| `gen_value_int` | `int64_t`; `0` if not an int |
| `gen_value_float` | `double`; `0.0` if not a float |
| `gen_value_string` | BORROWED UTF-8; `NULL` if not a string |
| `gen_value_string_length` | byte length |
| `gen_value_reference` | BORROWED target name |
| `gen_value_list_count` / `gen_value_list_get` | BORROWED item; `GEN_ERR_INDEX` if out of range |
| `gen_value_object_count` / `gen_value_object_key` / `gen_value_object_get` / `gen_value_object_get_index` | keys in insertion order |

`gen_value_clone` — deep copy, **OWNED**.

`gen_value_format` — GenLang text, **OWNED** string.

`gen_value_to_json` / `gen_value_to_yaml` — interchange text, **OWNED** string.

`gen_value_free` — frees an owned value. Do not free borrowed pointers from `gen_entity_value` or list/object getters.

## Paths and queries

### `gen_get` / `gen_eval_path`

Evaluate `ident{.prop|[index]}…`. The result is an **OWNED** clone.

Errors: `GEN_ERR_NOT_FOUND`, `GEN_ERR_INDEX`, `GEN_ERR_TYPE`, `GEN_ERR_QUERY`, `GEN_ERR_INVALID_ARGUMENT`.

### Query results

`gen_types_of`, `gen_ancestors_of`, `gen_descendants_of`, `gen_memberships_of`, `gen_set_members`, `gen_type_path`, `gen_search` return an **OWNED** `GenQueryResult`.

- `gen_types_of(entity)` — declared type then ancestors
- `gen_ancestors_of(type)` — parents only (not self)
- `gen_descendants_of(type)` — children, declaration order, recursive
- `gen_memberships_of(entity)` — sets the entity belongs to
- `gen_set_members(set)` — member entity names
- `gen_is_member(set, entity, &flag)` — `flag` is `0` or `1`; unknown set is `GEN_ERR_NOT_FOUND`
- `gen_type_path(from, to)` — chain if one type is an ancestor of the other
- `gen_search(text)` — case-sensitive substring over type/set/entity names and nested string values; sorted

`gen_query_result_count`, `gen_query_result_name` (BORROWED until the result is freed), `gen_query_result_free`.

### Format helpers

`gen_format_dyaz` / `gen_format_goster` return **OWNED** strings used by the CLI. Types and sets are never mixed in `dyaz` output.

## Minimal example

```c
GenContext *ctx = gen_context_create();
GenDocument *doc = NULL;
GenResult rc = gen_document_parse(ctx, "veri n = 1\n", &doc);
if (rc != GEN_OK) {
    fprintf(stderr, "%s\n", gen_error_message(gen_context_last_error(ctx)));
}
gen_document_free(doc);
gen_context_free(ctx);
```
