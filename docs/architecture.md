# GenLang architecture

## Pipeline

```text
GenLang source
      │
      ▼
    Lexer          handwritten, keyword table in lexer.c
      │
      ▼
    Parser         recursive descent, no generator
      │
      ▼
     AST           arena-allocated, source locations on every node
      │
      ▼
   iceaktar        load local `.gl` files, splice declarations, detect cycles
      │
      ▼
 Semantic analyzer  names, parents, cycles, memberships, references
      │
      ▼
  GenDocument
   /    |    \
types  sets  values
   \    |    /
      Query
        │
   Serialization
```

The AST is discarded after a document is built. Runtime objects do not alias parser nodes. That split is what later formatters, language servers, and alternative serializers can attach to without rewriting the document model.

JSON, YAML, and compact binary converters (`src/serializer/json*.c`, `yaml*.c`, `binary.c`) walk the runtime document and do not re-enter the parser except on import, which emits GenLang text and parses it.

## Type hierarchy vs set membership

`SUBTYPE_OF` is a relation between types:

```text
Kedi  SUBTYPE_OF  Memeli
Memeli SUBTYPE_OF Hayvan
Hayvan SUBTYPE_OF Canli
```

`TYPE_OF` relates an entity to one type:

```text
boncuk  TYPE_OF  Kedi
```

`MEMBER_OF` is independent:

```text
boncuk  MEMBER_OF  EvcilHayvanlar
boncuk  MEMBER_OF  SiyahHayvanlar
```

The analyzer never derives one family of relations from the other. `dyaz` prints **Types** and **Sets** as separate sections for that reason.

Internal relation tags also include `HAS_PROPERTY`, `CONTAINS`, and `REFERENCES`. They are not exposed through the public ABI.

## Runtime document

A `GenDocument` owns:

- type nodes (genus or species), hashed by name, ordered by declaration
- set nodes, hashed by name, ordered by declaration
- entities with an optional type and a tagged-union value
- membership records in declaration order
- an internal relation list

Values are tagged unions: null, bool, `int64_t`, `double`, UTF-8 string, list, object, reference. Object keys keep insertion order for deterministic serialization.

## Memory ownership

- The parser allocates AST nodes from an arena and frees the arena on success or failure.
- The document uses explicit `malloc`/`free` with matching destructors.
- Public `gen_*_free` functions are the only cleanup entry points bindings should call.
- File I/O lives in `src/io/file.c`. The parser accepts memory buffers so bindings never need `FILE *`.

## C ABI

`include/genlang.h` exposes opaque structs only. Shared-library builds hide non-exported symbols (`GENLANG_API`). Internals (`uthash`, vectors, arenas) are not installed.

The implementation can change hash tables, arenas, or graph layout without breaking bindings as long as the function signatures in `genlang.h` remain stable.

## CLI

`cli/` links `libgenlang` and calls the public API. Command dispatch in the REPL is a thin string match over library queries. Declarations typed in an empty REPL are concatenated and re-parsed through `gen_document_parse`.

## Threading

No global mutable library state. Distinct `GenContext` objects may be used from different threads. The same `GenDocument` must not be mutated concurrently. Read-only concurrent use is not a supported guarantee in 0.1.0.

## Resource limits

Each context carries limits (source size, nesting depth, string length, object properties, list length) with conservative defaults. The parser tracks nesting depth so maliciously deep documents fail with an error instead of overflowing the stack.

## Security

Parsing GenLang does not execute code, run a shell, touch the network, or load plugins. Untrusted files are still subject to resource exhaustion; use context limits.
