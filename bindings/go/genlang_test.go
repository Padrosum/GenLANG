package genlang

import (
	"path/filepath"
	"runtime"
	"testing"
)

const src = `
cins Canli
cins Hayvan -> Canli
tur Kedi -> Hayvan
kume Evcil
veri boncuk : Kedi {
    isim = "Boncuk"
    yas = 4
}
uye boncuk -> Evcil
`

func TestVersion(t *testing.T) {
	if Version() != "0.1.0" {
		t.Fatalf("version %q", Version())
	}
}

func TestParseAndQuery(t *testing.T) {
	doc, err := Parse(src)
	if err != nil {
		t.Fatal(err)
	}
	defer doc.Close()
	types := doc.Types()
	if len(types) != 3 || types[0] != "Canli" {
		t.Fatalf("types %v", types)
	}
	parent, ok, err := doc.TypeParent("Hayvan")
	if err != nil || !ok || parent != "Canli" {
		t.Fatalf("parent %q %v %v", parent, ok, err)
	}
	anc, err := doc.Ancestors("Kedi")
	if err != nil || len(anc) != 2 || anc[0] != "Hayvan" {
		t.Fatalf("ancestors %v %v", anc, err)
	}
	ty, ok, err := doc.EntityType("boncuk")
	if err != nil || !ok || ty != "Kedi" {
		t.Fatalf("entity type %q %v %v", ty, ok, err)
	}
	member, err := doc.IsMember("Evcil", "boncuk")
	if err != nil || !member {
		t.Fatalf("member %v %v", member, err)
	}
	val, err := doc.Entity("boncuk")
	if err != nil {
		t.Fatal(err)
	}
	obj, ok := val.(map[string]any)
	if !ok || obj["yas"].(int64) != 4 {
		t.Fatalf("entity %#v", val)
	}
	text, err := doc.Serialize()
	if err != nil || text == "" {
		t.Fatalf("serialize %v %v", text, err)
	}
	yaml, err := doc.ToYAML()
	if err != nil || yaml == "" {
		t.Fatalf("yaml %v %v", yaml, err)
	}
}

func TestNestedPath(t *testing.T) {
	doc, err := Parse("veri x { a = [ { b = [10, 20, 30] }, { b = [40, 50, 60] } ] }\n")
	if err != nil {
		t.Fatal(err)
	}
	defer doc.Close()
	v, err := doc.Get("x.a[1].b[2]")
	if err != nil || v.(int64) != 60 {
		t.Fatalf("path %v %v", v, err)
	}
}

func TestParseError(t *testing.T) {
	_, err := Parse("cins\n")
	if err == nil {
		t.Fatal("expected error")
	}
	ge, ok := err.(*Error)
	if !ok || ge.Code != int(5) {
		t.Fatalf("error %#v", err)
	}
}

func TestLoadExample(t *testing.T) {
	_, file, _, _ := runtime.Caller(0)
	path := filepath.Join(filepath.Dir(file), "..", "..", "examples", "animals.gl")
	doc, err := Load(path)
	if err != nil {
		t.Fatal(err)
	}
	defer doc.Close()
	ok, err := doc.IsMember("SiyahHayvanlar", "boncuk")
	if err != nil || !ok {
		t.Fatalf("membership %v %v", ok, err)
	}
}
