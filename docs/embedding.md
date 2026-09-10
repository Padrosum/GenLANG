# Embedding GenLang

`libgenlang` is the product. Every language below talks to the same C ABI in `include/genlang.h`.

Typical lifecycle:

```text
create context → load/parse source → validate (parse already validates)
→ query / read values → free results → free document → free context
```

Never keep borrowed pointers after the owner is freed.

## C

```c
#include <genlang.h>

GenContext *ctx = gen_context_create();
GenDocument *doc = NULL;
if (gen_document_parse(ctx, source, &doc) != GEN_OK) {
    /* inspect gen_context_last_error(ctx) */
}
/* gen_get, gen_types_of, gen_document_to_json / gen_document_to_yaml / gen_document_to_binary, … */
gen_document_free(doc);
gen_context_free(ctx);
```

C++ can call the same API inside `extern "C"` (already wrapped in the header).

## Rust

The crate in `bindings/rust` wraps the C ABI with `Drop`. After building `libgenlang`:

```bash
GENLANG_LIB_DIR=build cargo test --manifest-path bindings/rust/Cargo.toml
```

```rust
use genlang::Document;

let doc = Document::parse("cins T\nveri n : T { x = 1 }\n")?;
assert_eq!(doc.entity_type("n")?, Some("T".into()));
```

Do not keep borrowed C pointers; the wrapper copies strings and values into owned Rust types.

## Python

The `cffi` package in `bindings/python` loads `libgenlang` in ABI mode (`GENLANG_LIBRARY` or `GENLANG_LIB_DIR`, otherwise `build/libgenlang.so`).

```bash
PYTHONPATH=bindings/python/src GENLANG_LIB_DIR=build python3 -c "import genlang; print(genlang.version())"
```

```python
import genlang

doc = genlang.parse("cins T\nveri n : T { x = 1 }\n")
print(doc.entity("n"))  # {'x': 1}
doc.close()
```

Copied Python objects stay valid after `close()`; do not hold raw `cdata` pointers.

## Go

The `cgo` module in `bindings/go` links `libgenlang` (in-tree tests use `bindings/../../build`).

```go
doc, err := genlang.Parse("cins T\nveri n : T { x = 1 }\n")
defer doc.Close()
```

Copy `C.GoString` immediately; the wrapper already does that for names and values.

## Node.js / TypeScript

The N-API addon is `bindings/node` (`genlang.node`, built by CMake when `node_api.h` is found). It talks only to `include/genlang.h`.

```bash
GENLANG_NODE_MODULE=build/genlang.node node -e "const g=require('./bindings/node'); console.log(g.version())"
```

```js
const genlang = require("./bindings/node");
const doc = genlang.parse("cins T\nveri n : T { x = 1 }\n");
console.log(doc.entity("n")); // { x: 1 }
doc.close();
```

The addon holds `GenContext *` / `GenDocument *` and frees them on `close()` or garbage collection. Copied JS values stay valid after `close()`.

## Language server

`lsp/genlang_lsp.py` speaks LSP over stdio (diagnostics, hover, definition). It uses the Python cffi bindings.

```bash
PYTHONPATH=bindings/python/src GENLANG_LIB_DIR=build python3 lsp/genlang_lsp.py
```

Point the editor's GenLang language server command at that script.

## Java (JNA)

The package in `bindings/java` loads `libgenlang` with JNA. Copied `List` / `Map` / `Ref` values stay valid after `close()`.

```bash
# javac + jna.jar; LD_LIBRARY_PATH or -Djna.library.path must include libgenlang
GENLANG_LIB_DIR=build GENLANG_JNA_JAR=build/jna/jna.jar bash bindings/java/run-tests.sh
```

```java
try (genlang.Document doc = genlang.Document.parse("cins T\nveri n : T { x = 1 }\n")) {
    Object n = doc.entity("n");
}
```

Use `Document.load(path)` when the source contains `iceaktar`. UTF-8 names go through explicit NUL-terminated buffers, not JNA's default `String` marshalling.

## C# (P/Invoke)

```csharp
[DllImport("genlang")] static extern IntPtr gen_context_create();
[DllImport("genlang")] static extern int gen_document_parse(IntPtr ctx, string source, out IntPtr doc);
[DllImport("genlang")] static extern void gen_document_free(IntPtr doc);
[DllImport("genlang")] static extern void gen_context_free(IntPtr ctx);
```

Use `UTF8` string marshalling. Free owned pointers explicitly (or via `SafeHandle`).

The in-tree package is `bindings/csharp`: `Document.Parse` / `Load`, `IDisposable`, and copied values.

```csharp
using var doc = GenLang.Document.Parse("cins T\nveri n : T { x = 1 }\n");
```

`LD_LIBRARY_PATH` (or the Windows equivalent) must include `libgenlang`.
