#!/usr/bin/env python3

from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "bindings" / "python" / "src"))

import genlang


SRC = """
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
"""


class TestGenLang(unittest.TestCase):
    def test_version(self) -> None:
        self.assertEqual(genlang.version(), "0.1.0")

    def test_parse_query_and_sets(self) -> None:
        doc = genlang.parse(SRC)
        self.addCleanup(doc.close)
        self.assertEqual(doc.types, ["Canli", "Hayvan", "Kedi"])
        self.assertEqual(doc.type_parent("Hayvan"), "Canli")
        self.assertEqual(doc.ancestors("Kedi"), ["Hayvan", "Canli"])
        self.assertEqual(doc.entity_type("boncuk"), "Kedi")
        self.assertEqual(doc.entity("boncuk")["yas"], 4)
        self.assertTrue(doc.is_member("Evcil", "boncuk"))
        self.assertEqual(doc.members("Evcil"), ["boncuk"])
        self.assertIn("Kedi", doc.types_of("boncuk"))
        self.assertIn("Evcil", doc.memberships("boncuk"))
        self.assertIn("cins Canli", doc.serialize())
        self.assertIn("boncuk", doc.to_json())
        self.assertIn("boncuk", doc.to_yaml())
        roundtrip = genlang.from_yaml(doc.to_yaml())
        self.addCleanup(roundtrip.close)
        self.assertEqual(roundtrip.entities, doc.entities)
        binary = genlang.from_binary(doc.to_binary())
        self.addCleanup(binary.close)
        self.assertEqual(binary.entities, doc.entities)

    def test_nested_path(self) -> None:
        doc = genlang.parse(
            "veri x { a = [ { b = [10, 20, 30] }, { b = [40, 50, 60] } ] }\n"
        )
        self.addCleanup(doc.close)
        self.assertEqual(doc.get("x.a[1].b[2]"), 60)

    def test_unicode_ident(self) -> None:
        doc = genlang.parse("cins Canlı\nveri böncü : Canlı { n = 1 }\n")
        self.addCleanup(doc.close)
        self.assertEqual(doc.types, ["Canlı"])
        self.assertEqual(doc.entity("böncü")["n"], 1)

    def test_parse_error(self) -> None:
        with self.assertRaises(genlang.GenLangError) as caught:
            genlang.parse("cins\n")
        self.assertEqual(caught.exception.code, 5)  # GEN_ERR_PARSE

    def test_load_example(self) -> None:
        path = ROOT / "examples" / "animals.gl"
        doc = genlang.load(str(path))
        self.addCleanup(doc.close)
        self.assertIn("boncuk", doc.entities)
        self.assertTrue(doc.is_member("SiyahHayvanlar", "boncuk"))


if __name__ == "__main__":
    unittest.main()
