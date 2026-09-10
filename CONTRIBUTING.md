# Contributing to GenLang

Thank you for contributing. The library is the product; the CLI is only a consumer of `libgenlang`.

## Development

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Sanitizers:

```bash
cmake -S . -B build-san -DGENLANG_ENABLE_SANITIZERS=ON
cmake --build build-san
ctest --test-dir build-san
```

## Guidelines

- Keep the public ABI in `include/genlang.h` opaque and stable.
- Do not put parser or runtime logic in `cli/`.
- Do not infer type hierarchy from set membership, or the reverse.
- Preserve deterministic output (declaration order or explicit sorting).
- Match `docs/grammar.md` to the parser.
- Keep [The GenLang Book](docs/book/genlang-book.md) in sync with language and ABI changes; it is the canonical learning and usage text. Rebuild the PDF with `bash docs/book/build-pdf.sh` when the book source changes.
- Add tests for lexer, parser, semantics, runtime, query, serialization, and the public C API.
- Language bindings in `bindings/` must use only `include/genlang.h` and must copy borrowed strings before the document is freed.
- Keep the core free of network access, shell execution, and dynamic code loading.

## Code style

- C17, no compiler-specific extensions in the core
- Explicit ownership; every owned pointer has a matching `gen_*_free`
- Warning-clean under GCC/Clang with the project's warning flags

## License

Contributions are accepted under the [MIT License](LICENSE).
