# GenLang

[![License: MIT](https://img.shields.io/badge/license-MIT-0b6e4f.svg)](LICENSE)
[![C17](https://img.shields.io/badge/C-17-00599C.svg)](include/genlang.h)
[![Release](https://img.shields.io/badge/release-0.1.0-3d5a80.svg)](CHANGELOG.md)

Bildirimsel bir veri dili ve **gömülebilir C kitaplığı**. Hiyerarşik türler (cins / tür), bağımsız küme üyeliği, yapılandırılmış değerler, referanslar ve sorgular.

A declarative data language and an **embeddable C library**. Hierarchical types (genus / species), independent set membership, structured values, references, and queries.

JSON’un evrensel yerine geçmez. Tür zinciri ile etiketlerin *aynı anda, karışmadan* durması gerektiğinde uygulamanın yanında durur.

**Ürün kütüphanedir. CLI yalnızca tüketicidir.**

```text
libgenlang  =  çekirdek ürün     (opak C ABI)
genlang     =  komut satırı      (yalnızca include/genlang.h)
```

---

## Neden ayrı tutuyoruz?

JSON `boncuk` nesnesini yazar. Şunları native olarak söyleyemez:

- `Kedi`, `Memeli` → `Hayvan` → `Canli` zincirinde bir türdür
- `boncuk` bir `Kedi` örneğidir
- `boncuk` ayrıca `EvcilHayvanlar` ve `SiyahHayvanlar` kümelerindedir

Bunlar farklı ilişkilerdir:

```text
Tür hiyerarşisi (SUBTYPE_OF / TYPE_OF)

  Kedi → Memeli → Hayvan → Canli

Küme üyeliği (MEMBER_OF) — türlerden bağımsız

  boncuk ∈ EvcilHayvanlar
  boncuk ∈ SiyahHayvanlar
```

Tür ataları kümelerden türetilmez. Küme üyeliği türlerden türetilmez.

---

## Dil

| Kavram | Anahtar sözcük | Anlam |
| --- | --- | --- |
| Cins | `cins` | Hiyerarşide genel tip |
| Tür | `tur` | Daha özgül tip |
| Küme | `kume` | Sırasız üye koleksiyonu |
| Veri | `veri` | İsteğe bağlı tipli adlandırılmış değer |
| Üyelik | `uye` | Bağımsız `MEMBER_OF` |
| İçe aktar | `iceaktar` | Yerel `.gl` dosyası (kod çalışmaz) |

Değerler: `null`, `true` / `false`, tam sayı, kayan nokta, UTF-8 dizge, liste, nesne, `@referans`. Tanımlayıcılar UTF-8’dir (`Canlı` geçerlidir). Anahtar sözcükler ASCII kalır.

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

İç içe yol: `x.a[1].b[2]`. Başka dosya: `iceaktar "types.gl"` (göreli, yalnızca `.gl`, bir kez, döngü yok).

Daha fazla örnek: [`examples/`](examples/)

---

## Kurulum

CMake 3.16+, C17 derleyici, standart C kütüphanesi. `uthash` vendored.

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

CMake tüketicisi:

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

Çıkış kodları: `0` başarı, `1` genel, `2` sözdizimi, `3` anlamsal, `4` I/O.

REPL (hepsi kütüphane üzerinden): `dyaz`, `goster`, `uyeler`, `icerir`, `ustler`, `altlar`, `yol`, `ara`, `liste`, `yardim`, `temizle`, `cikis`.

---

## C kitaplığı

```c
#include <genlang.h>
#include <stdio.h>

int main(void)
{
    GenContext *ctx = gen_context_create();
    GenDocument *doc = NULL;
    GenValue *value = NULL;
    GenQueryResult *types = NULL;

    if (gen_document_load_file(ctx, "examples/animals.gl", &doc) != GEN_OK) {
        fprintf(stderr, "%s\n", gen_error_message(gen_context_last_error(ctx)));
        gen_context_free(ctx);
        return 1;
    }

    if (gen_get(doc, "boncuk.yas", &value) == GEN_OK) {
        printf("yas = %lld\n", (long long)gen_value_int(value));
        gen_value_free(value);
    }

    if (gen_types_of(doc, "boncuk", &types) == GEN_OK) {
        for (size_t i = 0; i < gen_query_result_count(types); i++) {
            printf("%s\n", gen_query_result_name(types, i));
        }
        gen_query_result_free(types);
    }

    gen_document_free(doc);
    gen_context_free(ctx);
    return 0;
}
```

- **OWNED** — bağlam, belge, `gen_get` değeri, sorgu sonucu, serileştirilmiş metin. Eşleşen `gen_*_free` / `gen_string_free`.
- **BORROWED** — belgeden alınan adlar ve `const GenValue *`. Belge yok edilene kadar geçerli.

Global durum yoktur. Ayrı `GenContext` nesneleri farklı iş parçacıklarından kullanılabilir. Aynı `GenDocument` üzerinde eşzamanlı yazma desteklenmez.

Tam API: [`docs/api.md`](docs/api.md)

---

## Diğer diller

Bağlayıcılar yalnızca `include/genlang.h` + `libgenlang` konuşur.

| Dil | Yol | Yöntem |
| --- | --- | --- |
| C / C++ | `include/genlang.h` | public ABI |
| Python | `bindings/python` | `cffi` |
| Rust | `bindings/rust` | `Drop` sarmalayıcı |
| Go | `bindings/go` | `cgo` |
| Node.js | `bindings/node` | N-API |
| Java | `bindings/java` | JNA |
| C# | `bindings/csharp` | P/Invoke |

```bash
PYTHONPATH=bindings/python/src GENLANG_LIB_DIR=build python3 -c "import genlang; print(genlang.version())"
```

Ayrıntı: [`docs/embedding.md`](docs/embedding.md)

---

## Mimari

```text
Kaynak → Lexer → Parser → AST → Anlamsal çözümleme → GenDocument
                                              /    |    \
                                      Tür grafı  Kümeler  Değerler
                                              \    |    /
                                                Sorgu → Serileştirme
```

CLI ayrıştırıcı veya çalışma zamanı içermez. [`docs/architecture.md`](docs/architecture.md)

```text
include/genlang.h     public ABI
src/                  kütüphane
cli/                  genlang yürütülebiliri
bindings/             Python, Rust, Go, Node, C#, Java
lsp/                  dil sunucusu
tests/                CTest
examples/             .gl örnekleri
docs/                 dil, API, gömme, AI kılavuzu
cmake/                CMake / pkg-config
third_party/uthash/   vendored (public API değil)
```

---

## Yol haritası

0.1.0 kullanılabilir bir MVP’dir. Sonraki iş aynı C ABI’nin arkasında kalır.

| Sürüm | Durum |
| --- | --- |
| 0.1 Foundation | çekirdek kütüphane, CLI, CTest |
| 0.2 Tooling | query, format, JSON, çoklu hata |
| 0.3 Growth | `iceaktar`, şema, Unicode, Python / Rust / Go |
| 0.4 Editor & interop | LSP, YAML, binary, Node, C#, Java |
| 1.0 | WASM, ABI politikası, eşzamanlı salt okuma |

**Kapsam dışı:** `.gl` içinde kabuk / ağ / kod; tür ↔ küme çıkarımı; JSON’u genel format olarak değiştirmek.

---

## Belgelendirme

- [Kullanım kılavuzu (Türkçe / English)](docs/usage.md)
- [AI kılavuzu](docs/ai-guide.md) — dil üreten ve tüketen modeller için
- [Dilbilgisi](docs/grammar.md)
- [C API](docs/api.md)
- [Mimari](docs/architecture.md)
- [Gömme](docs/embedding.md)
- [Değişiklikler](CHANGELOG.md)
- [Katkı](CONTRIBUTING.md)

---

## Lisans

[MIT](LICENSE). Telif: Alihan Karakuş, 2026.

`uthash` kendi BSD-tarzı lisansı ile [`third_party/uthash/`](third_party/uthash/) altındadır.
