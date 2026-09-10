package genlang;

import java.nio.file.Path;
import java.util.List;
import java.util.Map;

public final class TestGenLang {
    private static final String SRC =
            "cins Canli\n"
                    + "cins Hayvan -> Canli\n"
                    + "tur Kedi -> Hayvan\n"
                    + "kume Evcil\n"
                    + "veri boncuk : Kedi {\n"
                    + "    isim = \"Boncuk\"\n"
                    + "    yas = 4\n"
                    + "}\n"
                    + "uye boncuk -> Evcil\n";

    private static void expect(boolean cond, String message) {
        if (!cond) {
            System.err.println("FAIL " + message);
            System.exit(1);
        }
    }

    public static void main(String[] args) {
        expect("0.1.0".equals(Document.version()), "version");

        try (Document doc = Document.parse(SRC)) {
            expect(doc.types().equals(List.of("Canli", "Hayvan", "Kedi")), "types");
            expect("Canli".equals(doc.typeParent("Hayvan")), "parent");
            expect(doc.ancestors("Kedi").equals(List.of("Hayvan", "Canli")), "ancestors");
            expect("Kedi".equals(doc.entityType("boncuk")), "entity type");
            expect(doc.isMember("Evcil", "boncuk"), "member");
            @SuppressWarnings("unchecked")
            Map<String, Object> val = (Map<String, Object>) doc.entity("boncuk");
            expect(val != null && Long.valueOf(4).equals(val.get("yas")), "entity value");
            expect(doc.serialize().contains("cins Canli"), "serialize");
            expect(doc.toJson().contains("boncuk"), "json");
            expect(doc.toYaml().contains("boncuk"), "yaml");
        }

        try (Document doc = Document.parse(
                "veri x { a = [ { b = [10, 20, 30] }, { b = [40, 50, 60] } ] }\n"
        )) {
            expect(Long.valueOf(60).equals(doc.get("x.a[1].b[2]")), "nested path");
        }

        try {
            Document.parse("cins\n");
            expect(false, "expected parse error");
        } catch (GenLangException ex) {
            expect(ex.code == 5, "parse error code");
        }

        Path example = Path.of("examples", "animals.gl");
        try (Document doc = Document.load(example.toString())) {
            expect(doc.isMember("SiyahHayvanlar", "boncuk"), "load example");
        }

        System.out.println("ok");
    }
}
