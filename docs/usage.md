# GenLang Kullanım Kılavuzu / GenLang Usage Guide

## Türkçe

### 1. GenLang nedir?

GenLang, hiyerarşik türler (cins/tür), bağımsız küme üyeliği, yapılandırılmış değerler, referanslar ve sorgular üzerine kurulu küçük bir bildirime dayalı veri dilidir. Ürün **libgenlang** C kitaplığıdır; `genlang` komut satırı yalnızca bu kitaplığın tüketicisidir.

JSON'un evrensel yerine geçmesi amaçlanmaz. Uygulama yanında duran, JSON'dan daha zengin anlamsal ilişkilere sahip bir proje veri biçimidir.

### 2. Tasarım felsefesi

- Kütüphane önce, CLI sonra.
- Opak C ABI; diğer diller bağlanabilsin.
- Tür hiyerarşisi ile küme üyeliği asla karıştırılmaz.
- Gizli dönüşüm yok: referans kopya değildir, string sayı değildir.
- Açık bellek sahipliği, genel (global) durum yok.
- Ayrıştırma kod çalıştırmaz ve ağ veya kabuk kullanmaz. `iceaktar` yalnızca yerel `.gl` dosyalarını okur.

### 3. Cins / tür

```gl
cins Canli
cins Hayvan -> Canli
cins Memeli -> Hayvan
tur Kedi -> Memeli
```

`cins` genel, `tur` daha özgül bir tiptir. İkisi de aynı tür grafiğinde `SUBTYPE_OF` ilişkisiyle bağlanır.

### 4. Tür hiyerarşisi

```text
Kedi → Memeli → Hayvan → Canli
```

Bu **kalıtımdır**. `ustler Kedi` şunu üretir: `Memeli`, `Hayvan`, `Canli`.

### 5. Küme üyeliği

```gl
kume EvcilHayvanlar
kume SiyahHayvanlar
uye boncuk -> EvcilHayvanlar
uye boncuk -> SiyahHayvanlar
```

Bu **üyeliktir**. `SiyahHayvanlar` bir tür değildir.

### 6. Türler ile kümelerin farkı

| | Tür hiyerarşisi | Küme üyeliği |
| --- | --- | --- |
| İlişki | `SUBTYPE_OF` / `TYPE_OF` | `MEMBER_OF` |
| Örnek | `Kedi -> Memeli -> Hayvan -> Canli` | `boncuk -> EvcilHayvanlar` |
| Çıkarım | Yok | Yok |

Bir varlık aynı anda her ikisine sahip olabilir. `dyaz` bunları ayrı basar.

### 7. Veri bildirimleri

```gl
veri boncuk : Kedi {
    isim = "Boncuk"
    yas = 4
    renk = "siyah"
}

veri numbers = [10, 20, 30, 40]
veri ratio = 1.5
```

`: Kedi` isteğe bağlıdır ve `TYPE_OF` kurar.

Bir `cins`/`tur` özellik nesnesi verirse, o tipe bağlı `veri` aynı anahtarları ve değer türlerini sağlamalıdır. Eksik alan veya tür uyuşmazlığı anlamsal hatadır; fazla alan serbesttir. Üst tiplerin alanları da geçerlidir; daha özgül tip aynı anahtarı ezer. Özellik nesnesi olmayan tipler ek kısıt getirmez.

### 7.1 Dosya içe aktarma

```gl
iceaktar "types.gl"
tur Kedi -> Memeli
```

Yol, içe aktaran dosyaya görelidir ve `.gl` ile bitmelidir. Aynı dosya bir kez yüklenir; dosyalar arasında döngü `GEN_ERR_CYCLE` üretir. Kod çalışmaz. Bellekteki `gen_document_parse` içe aktarmayı reddeder; dosyadan yükleyin veya `gen_document_parse_at` kullanın.

### 8. Özellikler

Nesneler sırasız özellik koleksiyonudur; erişim `nesne.alan` iledir. Aynı isimde iki özellik yinelenme hatasıdır. Özellikler satır sonu veya isteğe bağlı virgülle ayrılabilir.

### 9. İlkel değerler

`null`, `true`, `false`, ondalıklı veya tam sayılar, UTF-8 dizgeler.

### 10–14. Listeler, nesneler, iç içe yapılar

Listeler sıralıdır, sıfır tabanlıdır. Dışarı taşan indeks `GEN_ERR_INDEX` döner.

```gl
veri x {
    a = [
        { b = [10, 20, 30] },
        { b = [40, 50, 60] }
    ]
}
```

`x.a[1].b[2]` → `60`.

### 15–17. Özellik, indeks, iç içe yol

```text
ident.ozellik
ident[indeks]
ident.ozellik[indeks].ozellik[indeks]
```

Sabit bir derinlik sınırı yoktur; bağlamın `max_nesting_depth` limiti geçerlidir.

### 18. Referanslar

```gl
veri owner = @ahmet
```

Referans, string değildir ve hedef nesnenin kopyasını oluşturmaz. Hedef varlık yoksa anlamsal hata oluşur.

### 19. İlişkiler

İçsel olarak: `SUBTYPE_OF`, `MEMBER_OF`, `TYPE_OF`, `HAS_PROPERTY`, `CONTAINS`, `REFERENCES`. Genel ABI grafik düğümlerini dışa açmaz.

### 20–27. Sözcüksel kurallar

- Yorum: `#` satır sonuna kadar.
- Tanımlayıcı: UTF-8. ASCII biçim `[A-Za-z_][A-Za-z0-9_]*`; `ı` / `ö` / `Canlı` gibi harfler de geçerlidir.
- Anahtar sözcükler: `cins tur kume veri uye iceaktar dyaz goster uyeler icerir ustler altlar yol ara liste yardim temizle cikis true false null`.
- Dizge kaçışları: `\" \\ \n \t \r`. Dizgeler geçerli UTF-8 olmalıdır.
- Tam sayı: `int64_t`. Kayan nokta: `double` (`1.0`, `1e10`).
- Boolean: `true` / `false`. Boş değer: `null`.

### 28. Dilbilgisi

Tam EBNF için `docs/grammar.md`. Ayrıştırıcı üreteci kullanılmaz.

### 29–32. Anlamsal kurallar

- Aynı ad alanında yinelenen tür / küme / varlık adı → `GEN_ERR_DUPLICATE`
- Bilinmeyen ebeveyn tür, varlık türü veya küme → `GEN_ERR_SEMANTIC`
- Çevrim ve öz-ebeveyn (`cins A -> A`) → `GEN_ERR_CYCLE`
- Geçersiz üyelik (yok varlık veya yok küme) → `GEN_ERR_SEMANTIC`
- İleri referanslı tür ebeveynleri geçerlidir (önce tüm türler toplanır).

### 33–42. Sorgu komutları

REPL veya eşdeğer C API:

| Komut | Anlam |
| --- | --- |
| `dyaz varlik` | Tür zinciri ve kümeler, ayrı başlıklarla |
| `goster varlik` | Tam değer |
| `uyeler Kume` | Üyeler |
| `icerir Kume x` | `true` / `false` |
| `ustler Tip` | Atalar (kendisi hariç) |
| `altlar Tip` | Alt türler |
| `yol A B` | Hiyerarşide yol varsa zincir |
| `ara "metin"` | Ada ve dizge değerlere büyük/küçük harf duyarlı alt dizge araması |
| `liste cins\|tur\|kume\|veri` | Bildirim sırası |

`dyaz` örnek çıktı:

```text
Types:
  Kedi
  Memeli
  Hayvan
  Canli

Sets:
  EvcilHayvanlar
  SiyahHayvanlar
```

### 43–45. REPL, CLI, doğrulama

```bash
genlang file.gl
genlang check file.gl
genlang repl [file.gl]
```

Çıkış kodları: `0` başarı, `1` genel, `2` sözdizimi, `3` anlamsal, `4` I/O.

Boş REPL'de `cins`/`veri`/`kume`/`uye` satırları biriktirilip yeniden ayrıştırılır. `yardim`, `temizle` (ANSI), `cikis`.

### 46. Serileştirme

`gen_document_serialize` belirleyici GenLang metni üretir. Aynı belge tekrar serileştirilince çıktı değişmez. Sıra: türler, kümeler, varlıklar, üyelikler (bildirim sırası). JSON, YAML ve kompakt ikili dönüşüm aynı şemayı kullanır (`types` / `sets` / `entities` / `memberships`); `genlang convert --json|--yaml|--binary`.

### 47–49. C API, sahiplik, hatalar

`include/genlang.h`. `GenResult` istisna kullanmaz. `gen_context_last_error` satır/sütun/ileti verir. Ayrıntı: `docs/api.md`.

OWNED: bağlam, belge, `gen_get` değeri, sorgu sonucu, serileştirilmiş metin.
BORROWED: belge içi adlar ve `gen_entity_value`.

### 50. İş parçacıkları

Global durum yoktur. Farklı `GenContext` nesneleri bağımsız kullanılabilir. Aynı `GenDocument` üzerinde eşzamanlı yazma desteklenmez.

### 51–58. Gömme ve diğer diller

C doğrudan `#include <genlang.h>`. C++ aynı C API. Bağlayıcılar: Python (`cffi`), Rust, Go (`cgo`), Node (N-API), Java (JNA), C# (P/Invoke). Örnekler: `docs/embedding.md`. Dil üreten veya tüketen modeller için: `docs/ai-guide.md`.

### 59. WebAssembly yol haritası

Çekirdek ayrıştırıcı POSIX'e bağlı değildir; Emscripten ile derleme ileride mümkündür. 0.1.0 bir WASM dağıtımı içermez.

### 60–61. Güvenlik ve kaynak sınırları

Ayrıştırma kabuk, ağ, dinamik kod veya gömülü betik çalıştırmaz. `iceaktar` yalnızca yerel `.gl` dosyalarını okur. Bağlam varsayılanları: 16 MiB kaynak, 256 iç içe derinlik, 1 MiB dizge, 65536 özellik/liste elemanı. `gen_context_set_limits` ile değiştirilir.

### 62. Tam örnek

`examples/animals.gl` ve `examples/nested.gl`.

### 63. Sık hatalar

| Belirti | Neden |
| --- | --- |
| `unknown type 'Foo'` | `veri x : Foo` ama `Foo` yok |
| `type cycle detected` | `A -> B -> A` veya `A -> A` |
| `duplicate type/entity/set` | Aynı ad ikinci kez |
| `expected ']'` | Liste virgülü veya kapanış eksik |
| `unterminated string` | Kapanmayan `"` |
| `unknown reference '@x'` | `@x` için varlık yok |
| `GEN_ERR_INDEX` | `liste[n]` taşması |

### 64. SSS

**JSON yerine neden GenLang?** Tür zinciri ve bağımsız kümeler JSON'un veri modelinde yoktur.

**Küme, tür müdür?** Hayır.

**`dyaz` kümeleri türlerin içine katar mı?** Hayır.

**Bağlayıcılar ne zaman?** ABI 0.1.0'da sabittir; bağlayıcılar ayrı iştir.

**Tanımlayıcılarda Türkçe harf?** Evet. İsimler UTF-8'dir (`Canlı`, `böncü`). Anahtar sözcükler ASCII kalır. Dizge değerleri de UTF-8'tir.

---

## English

### 1. What is GenLang?

GenLang is a small declarative data language built around hierarchical types (genus/species), independent set membership, structured values, references, and queries. The product is **libgenlang**, an embeddable C library. The `genlang` CLI only consumes that library.

It is not a universal JSON replacement. It is a project-oriented format for applications that need richer semantic relations than nested maps and arrays.

### 2. Design philosophy

- Library first, CLI second.
- Opaque C ABI so other languages can bind later.
- Type hierarchy and set membership are never mixed.
- No silent coercion: references are not copies; strings are not numbers.
- Explicit ownership, no global library state.
- Parsing does not execute code, touch the network, or invoke a shell. `iceaktar` may read local `.gl` files.

### 3. Genus / species

```gl
cins Canli
cins Hayvan -> Canli
cins Memeli -> Hayvan
tur Kedi -> Memeli
```

`cins` is a general type, `tur` a more specific one. Both live on the same type graph via `SUBTYPE_OF`.

### 4. Type hierarchy

```text
Kedi → Memeli → Hayvan → Canli
```

This is **inheritance**. `ustler Kedi` prints `Memeli`, `Hayvan`, `Canli`.

### 5. Set membership

```gl
kume EvcilHayvanlar
kume SiyahHayvanlar
uye boncuk -> EvcilHayvanlar
uye boncuk -> SiyahHayvanlar
```

This is **membership**. `SiyahHayvanlar` is not a type.

### 6. Types versus sets

| | Type hierarchy | Set membership |
| --- | --- | --- |
| Relation | `SUBTYPE_OF` / `TYPE_OF` | `MEMBER_OF` |
| Example | `Kedi -> Memeli -> Hayvan -> Canli` | `boncuk -> EvcilHayvanlar` |
| Inference | None | None |

An entity may have both at once. `dyaz` prints them in separate sections.

### 7. Data declarations

```gl
veri boncuk : Kedi {
    isim = "Boncuk"
    yas = 4
    renk = "siyah"
}

veri numbers = [10, 20, 30, 40]
veri ratio = 1.5
```

`: Kedi` is optional and establishes `TYPE_OF`.

If a `cins`/`tur` has a property object, typed `veri` must supply those keys with matching value kinds. Missing keys and kind mismatches are semantic errors; extra keys are allowed. Ancestor properties apply; a more specific type overrides the same key. Types without properties add no extra constraints.

### 7.1 File imports

```gl
iceaktar "types.gl"
tur Kedi -> Memeli
```

The path is resolved relative to the importing file and must end with `.gl`. A file is loaded once; a cycle among files is `GEN_ERR_CYCLE`. Nothing is executed. In-memory `gen_document_parse` rejects imports; load from a file or call `gen_document_parse_at`.

### 8. Properties

Objects are unordered property collections, accessed as `object.field`. Duplicate keys are an error. Properties may be separated by newlines and optional commas.

### 9. Primitive values

`null`, `true`, `false`, integers, floating-point numbers, UTF-8 strings.

### 10–14. Lists, objects, nesting

Lists are ordered and zero-based. Out-of-range access returns `GEN_ERR_INDEX`.

```gl
veri x {
    a = [
        { b = [10, 20, 30] },
        { b = [40, 50, 60] }
    ]
}
```

`x.a[1].b[2]` evaluates to `60`.

### 15–17. Property access, indexes, nested paths

```text
ident.property
ident[index]
ident.property[index].property[index]
```

There is no hard-coded nesting cap beyond the context `max_nesting_depth`.

### 18. References

```gl
veri owner = @ahmet
```

A reference is not a string and does not copy the target. A missing target is a semantic error.

### 19. Relations

Internally: `SUBTYPE_OF`, `MEMBER_OF`, `TYPE_OF`, `HAS_PROPERTY`, `CONTAINS`, `REFERENCES`. The public ABI does not expose the graph representation.

### 20–27. Lexical rules

- Comments: `#` to end of line.
- Identifiers: UTF-8. ASCII form `[A-Za-z_][A-Za-z0-9_]*`; non-ASCII letters are allowed.
- Keywords: `cins tur kume veri uye dyaz goster uyeler icerir ustler altlar yol ara liste yardim temizle cikis true false null`.
- String escapes: `\" \\ \n \t \r`. Strings must be valid UTF-8.
- Integers: `int64_t`. Floats: `double` (`1.0`, `1e10`).
- Booleans: `true` / `false`. Null: `null`.

### 28. Complete grammar

See `docs/grammar.md`. The parser is handwritten recursive descent.

### 29–32. Semantic rules

- Duplicate type / set / entity names → `GEN_ERR_DUPLICATE`
- Unknown parent type, entity type, or set → `GEN_ERR_SEMANTIC`
- Cycles and self-parenting (`cins A -> A`) → `GEN_ERR_CYCLE`
- Invalid membership → `GEN_ERR_SEMANTIC`
- Forward type parents are allowed (all types are collected first).

### 33–42. Query commands

REPL or the matching C API:

| Command | Meaning |
| --- | --- |
| `dyaz entity` | Type chain and sets, separate headings |
| `goster entity` | Full value |
| `uyeler Set` | Members |
| `icerir Set x` | `true` / `false` |
| `ustler Type` | Ancestors (excluding self) |
| `altlar Type` | Descendants |
| `yol A B` | Hierarchy path if one exists |
| `ara "text"` | Case-sensitive substring search over names and string values |
| `liste cins\|tur\|kume\|veri` | Declaration order |

`dyaz` sample:

```text
Types:
  Kedi
  Memeli
  Hayvan
  Canli

Sets:
  EvcilHayvanlar
  SiyahHayvanlar
```

### 43–45. REPL, CLI, validation

```bash
genlang file.gl
genlang check file.gl
genlang repl [file.gl]
```

Exit codes: `0` success, `1` general, `2` syntax, `3` semantic, `4` I/O.

An empty REPL accumulates `cins` / `veri` / `kume` / `uye` lines and re-parses them. `yardim`, `temizle` (ANSI clear), `cikis`.

### 46. Serialization

`gen_document_serialize` emits deterministic GenLang. Repeating it on an unchanged document yields the same bytes. Order: types, sets, entities, memberships (declaration order). JSON, YAML, and compact binary conversion share that schema (`types` / `sets` / `entities` / `memberships`); `genlang convert --json|--yaml|--binary`.

### 47–49. C API, ownership, errors

See `include/genlang.h` and `docs/api.md`. `GenResult` is not an exception. `gen_context_last_error` exposes line, column, and message.

OWNED: context, document, `gen_get` values, query results, serialized text.
BORROWED: names inside a document and `gen_entity_value`.

### 50. Threading

No global mutable state. Distinct `GenContext` objects may be used independently. Concurrent mutation of the same `GenDocument` is not supported.

### 51–58. Embedding and other languages

C: `#include <genlang.h>`. C++: the same C API. Bindings: Python (`cffi`), Rust, Go (`cgo`), Node (N-API), Java (JNA), C# (P/Invoke). Snippets: `docs/embedding.md`. Models generating or consuming GenLang should follow `docs/ai-guide.md`.

### 59. WebAssembly roadmap

The core parser does not depend on POSIX, so an Emscripten build is a future option. 0.1.0 does not ship a WASM artifact.

### 60–61. Security and resource limits

Parsing does not run a shell, use the network, load dynamic code, or execute scripts. `iceaktar` may read local `.gl` files only. Default limits: 16 MiB source, 256 nesting depth, 1 MiB strings, 65536 object properties / list items. Change them with `gen_context_set_limits`.

### 62. Complete examples

`examples/animals.gl` and `examples/nested.gl`.

### 63. Common errors

| Symptom | Cause |
| --- | --- |
| `unknown type 'Foo'` | `veri x : Foo` without declaring `Foo` |
| `type cycle detected` | `A -> B -> A` or `A -> A` |
| `duplicate type/entity/set` | The same name declared twice |
| `expected ']'` | Missing list comma or closer |
| `unterminated string` | Missing `"` |
| `unknown reference '@x'` | No entity named `x` |
| `GEN_ERR_INDEX` | `list[n]` out of range |

### 64. FAQ

**Why not JSON?** JSON has no type ancestry and no independent set membership.

**Is a set a type?** No.

**Does `dyaz` merge sets into types?** No.

**Where are language bindings?** The ABI is stable in 0.1.0; bindings are separate work.

**Turkish letters in identifiers?** Yes. Names are UTF-8 (`Canlı`, `böncü`). Keywords stay ASCII. String values are also UTF-8.
