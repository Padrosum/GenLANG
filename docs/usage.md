# GenLang usage (short)

The full learning and usage text is **The GenLang Book**:

- [Markdown](book/genlang-book.md)
- [PDF](book/genlang-book.pdf)

This page is a compact bilingual tutorial. If it disagrees with the book, the book wins.

---

## Türkçe

GenLang, hiyerarşik türler (`cins` / `tur`), bağımsız küme üyeliği (`kume` / `uye`), değerler, referanslar ve sorgular için bildirime dayalı bir veri dilidir. Ürün **libgenlang** C kitaplığıdır; `genlang` CLI yalnızca bu kitaplığı kullanır.

JSON’un yerine geçmez. Tür zinciri ile etiketlerin *karışmadan* durması gerektiğinde uygulamanın yanında durur.

### İlk dosya

```gl
cins Canli
cins Hayvan -> Canli
tur Kedi -> Hayvan

kume EvcilHayvanlar

veri boncuk : Kedi {
    isim = "Boncuk"
    yas = 4
}

uye boncuk -> EvcilHayvanlar
```

```bash
./build/genlang check examples/animals.gl
./build/genlang query examples/animals.gl boncuk.yas
```

`Kedi → Hayvan → Canli` **tür** zinciridir. `EvcilHayvanlar` bir tür değildir; üyelik `uye` ile kurulur. İkisi asla birbirinden türetilmez.

### Sözcükler

Belge: `cins` `tur` `kume` `veri` `uye` `iceaktar`.  
Değer: `true` `false` `null`.  
REPL (komut olarak `.gl` dosyasında yok; isim olarak kullanılabilir): `dyaz` `goster` `uyeler` `icerir` `ustler` `altlar` `yol` `ara` `liste` `yardim` `temizle` `cikis`.

Nesneler `anahtar = değer` kullanır (JSON `:` değil). Listelerde virgül zorunludur. Referans `@ad` bir dizge değildir. Tanımlayıcılar UTF-8’dir (`Canlı`); anahtar sözcükler ASCII kalır.

`iceaktar "types.gl"` yalnızca yerel `.gl` okur. Bellekte `parse` içe aktarmayı reddeder; dosyadan `load` edin.

Şema: `cins`/`tur` özellik nesnesi varsa, o tipe bağlı `veri` aynı anahtarları ve **değer türlerini** sağlamalıdır. Fazla alan serbesttir. Şemada `@Kisi` (Kisi bir türse) hedef entity’nin o tür veya alt tür olmasını ister. `liste Kedi` o türün örneklerini listeler.

Ayrıntı, hatalar, CLI, ABI ve örnekler: [kitap](book/genlang-book.md).

---

## English

GenLang is a declarative data language for hierarchical types (`cins` / `tur`), independent set membership (`kume` / `uye`), values, references, and queries. The product is **libgenlang**; the `genlang` CLI only consumes it.

It is not a JSON replacement. Use it when a taxonomy and tags must coexist without being inferred from each other.

### First file

Same listing as above. Check and query with the CLI. `Kedi → Hayvan → Canli` is a **type** chain. `EvcilHayvanlar` is a set; membership is `uye`. Neither relation is derived from the other.

### Keywords

Documents: `cins` `tur` `kume` `veri` `uye` `iceaktar`.  
Literals: `true` `false` `null`.  
REPL only (illegal as commands in `.gl` files; legal as names): `dyaz` `goster` `uyeler` `icerir` `ustler` `altlar` `yol` `ara` `liste` `yardim` `temizle` `cikis`.

Objects use `key = value` (not JSON `:`). Lists require commas. `@name` is a reference, not a string. Identifiers are UTF-8; keywords stay ASCII.

`iceaktar "types.gl"` reads local `.gl` files only. In-memory `parse` rejects imports; `load` a file.

If a type has a property object, typed `veri` must supply those keys with matching **kinds**. Extra keys are allowed. A schema `@Kisi` (when `Kisi` is a type) requires the target entity to have that type or a subtype. `liste Kedi` lists instances of a type.

Everything else — schemas, imports, paths, CLI, C ABI, bindings, interchange, errors — is in [the book](book/genlang-book.md).
