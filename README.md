# GenLang

[![License: MIT](https://img.shields.io/badge/license-MIT-0b6e4f.svg)](LICENSE)
[![C17](https://img.shields.io/badge/C-17-00599C.svg)](include/genlang.h)
[![Release](https://img.shields.io/badge/release-0.1.0-3d5a80.svg)](CHANGELOG.md)

A declarative data language and an **embeddable C library**. It models hierarchical types (genus / species), independent set membership, structured values, references, and queries.

It is not a universal JSON replacement. Keep it next to an application when you need a taxonomy and tags in the same document without mixing them.

**The library is the product. The CLI only consumes that library.**

```text
libgenlang  =  core product     (opaque C ABI)
genlang     =  CLI frontend     (include/genlang.h only)
```

Current release: **0.1.0** · License: **MIT**

### The GenLang Book

Canonical learning and usage text (34 pages):

| Format | In this repo | On GitHub |
| --- | --- | --- |
| Markdown | [`docs/book/genlang-book.md`](docs/book/genlang-book.md) | [view](https://github.com/Padrosum/GenLANG/blob/main/docs/book/genlang-book.md) |
| PDF | [`docs/book/genlang-book.pdf`](docs/book/genlang-book.pdf) | [view](https://github.com/Padrosum/GenLANG/blob/main/docs/book/genlang-book.pdf) · [raw download](https://github.com/Padrosum/GenLANG/raw/main/docs/book/genlang-book.pdf) |

---

## Why GenLang exists

JSON can represent `boncuk` as an object. It cannot natively say:

- `Kedi` is a species of `Memeli`, under `Hayvan`, under `Canli`
- `boncuk` is an instance of type `Kedi`
- `boncuk` also belongs to the sets `EvcilHayvanlar` and `SiyahHayvanlar`

Those are different relations. GenLang keeps them separate:

```text
Type hierarchy (SUBTYPE_OF / TYPE_OF)

  Kedi → Memeli → Hayvan → Canli

Set membership (MEMBER_OF), independent of types

  boncuk ∈ EvcilHayvanlar
  boncuk ∈ SiyahHayvanlar
```

Type ancestry is never inferred from sets. Set membership is never inferred from types.

---

## Language

| Concept | Keyword | Meaning |
| --- | --- | --- |
| Genus | `cins` | A general type in the hierarchy |
| Species | `tur` | A more specific type |
| Set | `kume` | An unordered collection of members |
| Data | `veri` | A named value, optionally typed |
| Membership | `uye` | Independent `MEMBER_OF` |
| Import | `iceaktar` | Local `.gl` file (no code execution) |

Values: `null`, booleans, integers, floats, UTF-8 strings, lists, objects, references (`@name`). Identifiers are UTF-8 (`Canlı` is valid). Keywords stay ASCII.

```gl
cins Canli
cins Hayvan -> Canli
cins Memeli -> Hayvan

tur Kedi -> Memeli
tur Kopek -> Memeli

kume EvcilHayvanlar
kume SiyahHayvanlar

veri boncuk : Kedi {
    isim = "Boncuk"
    yas = 4
    renk = "siyah"
}

uye boncuk -> EvcilHayvanlar
uye boncuk -> SiyahHayvanlar
```

Nested path: `x.a[1].b[2]`. Import: `iceaktar "types.gl"` (relative, `.gl` only, loaded once).

Samples: [`examples/`](examples/). Full rules: [The GenLang Book](docs/book/genlang-book.md).

---

## Build

Requires CMake 3.16+, a C17 compiler, and a standard C library. `uthash` is vendored.

```bash
git clone git@github.com:Padrosum/GenLANG.git
cd GenLANG
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

```bash
cmake --install build
```

```cmake
find_package(GenLang REQUIRED)
target_link_libraries(my_app PRIVATE GenLang::genlang)
```

```bash
pkg-config --cflags --libs genlang
```

---

## CLI

```bash
genlang examples/animals.gl
genlang check file.gl
genlang query file.gl boncuk.yas
genlang convert file.gl --json
genlang convert file.json --from-json
genlang convert file.gl --yaml
genlang convert file.yaml --from-yaml
genlang convert file.gl --binary
genlang convert file.bin --from-binary
genlang format file.gl
genlang format --in-place file.gl
genlang repl [file.gl]
```

Exit codes: `0` success, `1` general, `2` lex/parse, `3` semantic, `4` I/O.

REPL (library-backed): `dyaz`, `goster`, `uyeler`, `icerir`, `ustler`, `altlar`, `yol`, `ara`, `liste`, `yardim`, `temizle`, `cikis`.

---

## C library

```c
#include <genlang.h>
#include <stdio.h>

int main(void)
{
    GenContext *ctx = gen_context_create();
    GenDocument *doc = NULL;
    GenValue *value = NULL;

    if (gen_document_load_file(ctx, "examples/animals.gl", &doc) != GEN_OK) {
        fprintf(stderr, "%s\n", gen_error_message(gen_context_last_error(ctx)));
        gen_context_free(ctx);
        return 1;
    }

    if (gen_get(doc, "boncuk.yas", &value) == GEN_OK) {
        printf("yas = %lld\n", (long long)gen_value_int(value));
        gen_value_free(value);
    }

    gen_document_free(doc);
    gen_context_free(ctx);
    return 0;
}
```

- **OWNED** — context, document, `gen_get` values, query results, serialized text. Free with `gen_*_free` / `gen_string_free`.
- **BORROWED** — names and `const GenValue *` taken from a document. Valid until that document is freed.

No global context. Distinct `GenContext` objects may be used from different threads. Concurrent mutation of the same `GenDocument` is not supported.

Full API: [`docs/api.md`](docs/api.md). Book chapters 11–13 cover embedding.

---

## Other languages

Wrappers under [`bindings/`](bindings/) talk only to `include/genlang.h` + `libgenlang`:

| Language | Path | Approach |
| --- | --- | --- |
| C / C++ | `include/genlang.h` | public ABI |
| Python | `bindings/python` | `cffi` |
| Rust | `bindings/rust` | `Drop` wrappers |
| Go | `bindings/go` | `cgo` |
| Node.js | `bindings/node` | N-API |
| Java | `bindings/java` | JNA |
| C# | `bindings/csharp` | P/Invoke |

```bash
PYTHONPATH=bindings/python/src GENLANG_LIB_DIR=build python3 -c "import genlang; print(genlang.version())"
```

Details: [`docs/embedding.md`](docs/embedding.md).

---

## Architecture

```text
Source → Lexer → Parser → AST → Semantic analyzer → GenDocument
                                              /    |    \
                                      Type graph  Sets  Values
                                              \    |    /
                                                Query → Serialization
```

The CLI never implements parser or runtime logic. [`docs/architecture.md`](docs/architecture.md)

```text
include/genlang.h     public ABI
src/                  library
cli/                  genlang executable
bindings/             Python, Rust, Go, Node, C#, Java
lsp/                  language server
tests/                CTest
examples/             .gl samples
docs/                 book, grammar, API, embedding
docs/book/            The GenLang Book (Markdown + PDF)
cmake/                CMake / pkg-config
third_party/uthash/   vendored (not public API)
```

---

## Testing

```bash
ctest --test-dir build
```

The suite covers the lexer, parser, semantic analyzer, runtime, queries, serialization, the public C API, `examples/*.gl`, and language bindings when those tools are on `PATH`.

Rebuild the book PDF (optional; `pandoc` + WeasyPrint):

```bash
bash docs/book/build-pdf.sh
```

---

## Roadmap

The 0.1.0 line is a usable MVP. Later work stays behind the same C ABI.

| Series | Status |
| --- | --- |
| 0.1 Foundation | library, CLI, CTest |
| 0.2 Tooling | query, format, JSON, multi-error |
| 0.3 Growth | `iceaktar`, schemas, Unicode, Python / Rust / Go |
| 0.4 Editor & interop | LSP, YAML, binary, Node, C#, Java |
| 1.0 | WASM, ABI policy, concurrent read-only |

**Out of scope:** code, network, or shell in `.gl` files; inferring types from sets (or the reverse); replacing JSON as a general format.

---

## Documentation

- **Book (Markdown):** [`docs/book/genlang-book.md`](docs/book/genlang-book.md) · [GitHub](https://github.com/Padrosum/GenLANG/blob/main/docs/book/genlang-book.md)
- **Book (PDF):** [`docs/book/genlang-book.pdf`](docs/book/genlang-book.pdf) · [GitHub](https://github.com/Padrosum/GenLANG/blob/main/docs/book/genlang-book.pdf) · [download](https://github.com/Padrosum/GenLANG/raw/main/docs/book/genlang-book.pdf)
- [Short usage guide (Türkçe / English)](docs/usage.md)
- [Grammar](docs/grammar.md)
- [C API](docs/api.md)
- [Architecture](docs/architecture.md)
- [Embedding](docs/embedding.md)
- [AI usage guide](docs/ai-guide.md)
- [Changelog](CHANGELOG.md)
- [Contributing](CONTRIBUTING.md)

---

## License

[MIT](LICENSE). Copyright © 2026 Alihan Karakuş.

`uthash` is vendored under its own BSD-style license in [`third_party/uthash/`](third_party/uthash/).

---

## Examples

Runnable copies live under [`examples/`](examples/). More in [the book](docs/book/genlang-book.md).

### Types, sets, and membership

[`examples/animals.gl`](examples/animals.gl)

```gl
cins Canli
cins Hayvan -> Canli
cins Memeli -> Hayvan

tur Kedi -> Memeli
tur Kopek -> Memeli

kume EvcilHayvanlar
kume SiyahHayvanlar

veri boncuk : Kedi {
    isim = "Boncuk"
    yas = 4
    renk = "siyah"
}

veri karamel : Kopek {
    isim = "Karamel"
    yas = 7
    renk = "kahverengi"
}

uye boncuk -> EvcilHayvanlar
uye boncuk -> SiyahHayvanlar
uye karamel -> EvcilHayvanlar
```

```bash
./build/genlang query examples/animals.gl boncuk.yas
# 4
```

### Nested paths

[`examples/nested.gl`](examples/nested.gl) — `x.a[1].b[2]` is `60`.

```gl
veri x {
    a = [
        { b = [10, 20, 30] },
        { b = [40, 50, 60] }
    ]
}
```

### Optional schemas

[`examples/schema.gl`](examples/schema.gl) — extra keys on the instance are allowed; kinds must match.

```gl
cins Kayit {
    id = ""
}

cins Hayvan -> Kayit {
    isim = ""
    yas = 0
}

tur Kedi -> Hayvan {
    renk = ""
}

veri boncuk : Kedi {
    id = "kedi-001"
    isim = "Boncuk"
    yas = 4
    renk = "siyah"
    ekstra = true
}
```

### References

[`examples/relations.gl`](examples/relations.gl) — `@ahmet` is a reference, not a string and not a copy. A schema `@Kisi` requires the target to be that type (or a subtype).

```gl
cins Kisi

cins Proje {
    sahibi = @Kisi
    etiketler = [""]
}

veri ahmet : Kisi {
    isim = "Ahmet"
}

veri owner = @ahmet

veri proje : Proje {
    sahibi = @ahmet
    etiketler = ["genlang", "veri"]
}
```

### Imports

[`examples/import/`](examples/import/) — load the file; in-memory `parse` rejects `iceaktar`.

```gl
iceaktar "types.gl"

tur Kedi -> Memeli

veri boncuk : Kedi {
    isim = "Boncuk"
}
```
