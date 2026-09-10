using GenLang;

const string src = """
cins Canli
cins Hayvan -> Canli
tur Kedi -> Hayvan
kume Evcil
veri boncuk : Kedi {
    isim = "Boncuk"
    yas = 4
}
uye boncuk -> Evcil
""";

static void Expect(bool cond, string message)
{
    if (!cond)
    {
        Console.Error.WriteLine("FAIL " + message);
        Environment.Exit(1);
    }
}

Expect(Document.Version() == "0.1.0", "version");

using (var doc = Document.Parse(src))
{
    Expect(doc.Types.Count == 3 && doc.Types[0] == "Canli", "types");
    Expect(doc.TypeParent("Hayvan") == "Canli", "parent");
    var anc = doc.Ancestors("Kedi");
    Expect(anc.Count == 2 && anc[0] == "Hayvan", "ancestors");
    Expect(doc.EntityType("boncuk") == "Kedi", "entity type");
    Expect(doc.IsMember("Evcil", "boncuk"), "member");
    var val = doc.Entity("boncuk") as Dictionary<string, object?>;
    Expect(val != null && val["yas"] is long yas && yas == 4, "entity value");
    Expect(doc.Serialize().Contains("cins Canli"), "serialize");
    Expect(doc.ToJson().Contains("boncuk"), "json");
    Expect(doc.ToYaml().Contains("boncuk"), "yaml");
}

using (var doc = Document.Parse("veri x { a = [ { b = [10, 20, 30] }, { b = [40, 50, 60] } ] }\n"))
{
    Expect(doc.Get("x.a[1].b[2]") is long n && n == 60, "nested path");
}

try
{
    Document.Parse("cins\n");
    Expect(false, "expected parse error");
}
catch (GenLangException ex)
{
    Expect(ex.Code == 5, "parse error code");
}

var example = Path.GetFullPath(Path.Combine(Directory.GetCurrentDirectory(), "examples", "animals.gl"));
using (var doc = Document.Load(example))
{
    Expect(doc.IsMember("SiyahHayvanlar", "boncuk"), "load example");
}

Console.WriteLine("ok");
