#!/usr/bin/env python3
"""Load examples/music.gl and print types vs playlists separately."""

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "bindings", "python", "src"))
os.environ.setdefault("GENLANG_LIB_DIR", os.path.join(ROOT, "build"))

import genlang  # noqa: E402

with genlang.load(os.path.join(ROOT, "examples", "music.gl")) as doc:
    print("version", genlang.version())
    print("parca_01 type:", doc.entity_type("parca_01"))
    print("ancestors:", doc.ancestors("Parca"))
    print("title:", doc.get("parca_01.baslik"))
    print("album ref:", doc.get("parca_01.album"))
    print("Gece members:", doc.members("Gece"))
    print("Favoriler has parca_01:", doc.is_member("Favoriler", "parca_01"))
    print("Favoriler has parca_02:", doc.is_member("Favoriler", "parca_02"))
