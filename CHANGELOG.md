# Changelog

All notable changes to this project are documented in this file.

## [0.1.0] — 2026-09-06

Initial public release of libgenlang and the genlang CLI.

- C17 embeddable library with an opaque, FFI-friendly ABI
- Handwritten lexer and recursive-descent parser
- Semantic analysis for genus/species hierarchies and independent set membership
- Nested path evaluation, queries, deterministic serialization
- CLI (`genlang`) and REPL as library consumers
- CMake install rules, pkg-config, and CTest suite
- `genlang query` and `genlang convert --json` (document JSON with types, sets, memberships)
- Canonical formatter (`genlang format` / `--in-place`)
- JSON import (`gen_document_from_json`, `genlang convert --from-json`)
- Type/set property queries (`gen_type_properties`, `gen_set_property`, …)
- Multi-error semantic reports (`gen_context_error_count` / `gen_context_error`; CLI prints all)
- File imports via `iceaktar "file.gl"` (`gen_document_load_file` / `gen_document_parse_at`)
- Optional type-property schemas: typed `veri` must match `cins`/`tur` property keys and kinds
- Unicode identifiers (UTF-8 names such as `Canlı`)
- Python (`cffi`), Rust, and Go bindings in `bindings/`
- Language server (`lsp/genlang_lsp.py`): diagnostics, hover, go-to-definition
- Declaration locations (`gen_type_location`, `gen_set_location`, `gen_entity_location`)
- YAML import/export (`gen_document_to_yaml` / `gen_document_from_yaml`, `genlang convert --yaml` / `--from-yaml`)
- Compact binary import/export (`gen_document_to_binary` / `gen_document_from_binary`, `genlang convert --binary` / `--from-binary`)
- Node.js N-API addon (`bindings/node`) and C# P/Invoke package (`bindings/csharp`)
- Java JNA wrapper (`bindings/java`)
- AI usage guide (`docs/ai-guide.md`) for generating and consuming GenLang
- The GenLang Book (`docs/book/genlang-book.md` and `docs/book/genlang-book.pdf`)
