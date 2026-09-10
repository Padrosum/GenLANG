"use strict";

const assert = require("assert");
const path = require("path");
const genlang = require("./index.js");

const SRC = `
cins Canli
cins Hayvan -> Canli
tur Kedi -> Hayvan
kume Evcil
veri boncuk : Kedi {
    isim = "Boncuk"
    yas = 4
    renk = "siyah"
}
uye boncuk -> Evcil
`;

assert.strictEqual(genlang.version(), "0.1.0");

{
  const doc = genlang.parse(SRC);
  assert.deepStrictEqual(doc.types, ["Canli", "Hayvan", "Kedi"]);
  assert.strictEqual(doc.typeParent("Hayvan"), "Canli");
  assert.deepStrictEqual(doc.ancestors("Kedi"), ["Hayvan", "Canli"]);
  assert.strictEqual(doc.entityType("boncuk"), "Kedi");
  assert.strictEqual(doc.entity("boncuk").yas, 4);
  assert.strictEqual(doc.isMember("Evcil", "boncuk"), true);
  assert.deepStrictEqual(doc.members("Evcil"), ["boncuk"]);
  assert.ok(doc.serialize().includes("cins Canli"));
  assert.ok(doc.toJson().includes("boncuk"));
  assert.ok(doc.toYaml().includes("boncuk"));
  assert.ok(Buffer.isBuffer(doc.toBinary()));
  assert.ok(doc.toBinary().length >= 4);
  doc.close();
}

{
  const doc = genlang.parse(
    "veri x { a = [ { b = [10, 20, 30] }, { b = [40, 50, 60] } ] }\n"
  );
  assert.strictEqual(doc.get("x.a[1].b[2]"), 60);
  doc.close();
}

{
  let threw = false;
  try {
    genlang.parse("cins\n");
  } catch (err) {
    threw = true;
    assert.strictEqual(err.code, 5);
  }
  assert.ok(threw);
}

{
  const doc = genlang.load(path.join(__dirname, "..", "..", "examples", "animals.gl"));
  assert.ok(doc.entities.includes("boncuk"));
  assert.strictEqual(doc.isMember("SiyahHayvanlar", "boncuk"), true);
  doc.close();
}

console.log("ok");
