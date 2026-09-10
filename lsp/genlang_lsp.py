#!/usr/bin/env python3
"""GenLang language server (JSON-RPC over stdio).

Provides diagnostics, hover, and go-to-definition using libgenlang.
"""

from __future__ import annotations

import json
import sys
import urllib.parse
from dataclasses import dataclass
from pathlib import Path
from typing import Any, TextIO

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "bindings" / "python" / "src"))

import genlang


def _uri_to_path(uri: str) -> str:
    if uri.startswith("file://"):
        parsed = urllib.parse.urlparse(uri)
        return urllib.parse.unquote(parsed.path)
    return uri


def _ident_at(text: str, line: int, character: int) -> str | None:
    lines = text.splitlines()
    if line < 0 or line >= len(lines):
        return None
    row = lines[line]
    if character < 0:
        character = 0
    if character > len(row):
        character = len(row)
    i = character
    while i > 0 and (row[i - 1].isalnum() or row[i - 1] == "_"):
        i -= 1
    j = character
    while j < len(row) and (row[j].isalnum() or row[j] == "_"):
        j += 1
    name = row[i:j]
    return name or None


def _pos(line: int, column: int) -> dict[str, int]:
    # GenLang locations are 1-based; LSP is 0-based.
    ln = max(int(line) - 1, 0)
    col = max(int(column) - 1, 0)
    return {"line": ln, "character": col}


@dataclass
class OpenFile:
    uri: str
    text: str
    doc: genlang.Document | None


class Server:
    def __init__(self, inf: TextIO, out: TextIO) -> None:
        self.inf = inf
        self.out = out
        self.files: dict[str, OpenFile] = {}

    def write(self, payload: dict[str, Any]) -> None:
        raw = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        header = f"Content-Length: {len(raw)}\r\n\r\n".encode("ascii")
        self.out.buffer.write(header + raw)
        self.out.buffer.flush()

    def read(self) -> dict[str, Any] | None:
        headers: dict[str, str] = {}
        while True:
            line = self.inf.buffer.readline()
            if not line:
                return None
            if line in (b"\r\n", b"\n"):
                break
            key, _, value = line.decode("ascii").partition(":")
            headers[key.strip().lower()] = value.strip()
        length = int(headers.get("content-length", "0"))
        body = self.inf.buffer.read(length)
        if not body:
            return None
        return json.loads(body.decode("utf-8"))

    def publish_diagnostics(self, uri: str, errors: list[genlang.GenLangError]) -> None:
        items = []
        for err in errors:
            start = _pos(err.line or 1, err.column or 1)
            items.append(
                {
                    "range": {"start": start, "end": start},
                    "severity": 1,
                    "source": "genlang",
                    "message": err.message,
                }
            )
        self.write(
            {
                "jsonrpc": "2.0",
                "method": "textDocument/publishDiagnostics",
                "params": {"uri": uri, "diagnostics": items},
            }
        )

    def analyze(self, uri: str, text: str) -> None:
        origin = _uri_to_path(uri)
        doc, errors = genlang.try_parse(text, origin)
        prev = self.files.get(uri)
        if prev and prev.doc is not None:
            prev.doc.close()
        self.files[uri] = OpenFile(uri=uri, text=text, doc=doc)
        self.publish_diagnostics(uri, errors)

    def hover(self, uri: str, line: int, character: int) -> dict[str, Any] | None:
        opened = self.files.get(uri)
        if opened is None or opened.doc is None:
            return None
        name = _ident_at(opened.text, line, character)
        if not name:
            return None
        bits: list[str] = []
        if name in opened.doc.types:
            parent = opened.doc.type_parent(name)
            anc = opened.doc.ancestors(name)
            bits.append(f"type `{name}`")
            if parent:
                bits.append(f"parent `{parent}`")
            if anc:
                bits.append("ancestors: " + ", ".join(f"`{a}`" for a in anc))
        if name in opened.doc.sets:
            bits.append(f"set `{name}`")
            bits.append("members: " + ", ".join(opened.doc.members(name)))
        if name in opened.doc.entities:
            ty = opened.doc.entity_type(name)
            bits.append(f"entity `{name}`")
            if ty:
                bits.append(f"type `{ty}`")
            sets = opened.doc.memberships(name)
            if sets:
                bits.append("sets: " + ", ".join(f"`{s}`" for s in sets))
        if not bits:
            return None
        return {"contents": {"kind": "markdown", "value": "\n\n".join(bits)}}

    def definition(self, uri: str, line: int, character: int) -> dict[str, Any] | None:
        opened = self.files.get(uri)
        if opened is None or opened.doc is None:
            return None
        name = _ident_at(opened.text, line, character)
        if not name:
            return None
        loc = None
        if name in opened.doc.types:
            loc = opened.doc.type_location(name)
        elif name in opened.doc.sets:
            loc = opened.doc.set_location(name)
        elif name in opened.doc.entities:
            loc = opened.doc.entity_location(name)
        if loc is None:
            return None
        start = _pos(loc[0], loc[1])
        return {"uri": uri, "range": {"start": start, "end": start}}

    def handle(self, msg: dict[str, Any]) -> None:
        method = msg.get("method")
        req_id = msg.get("id")
        params = msg.get("params") or {}
        if method == "initialize":
            self.write(
                {
                    "jsonrpc": "2.0",
                    "id": req_id,
                    "result": {
                        "capabilities": {
                            "textDocumentSync": 1,
                            "hoverProvider": True,
                            "definitionProvider": True,
                        },
                        "serverInfo": {"name": "genlang", "version": genlang.version()},
                    },
                }
            )
            return
        if method == "initialized":
            return
        if method == "shutdown":
            self.write({"jsonrpc": "2.0", "id": req_id, "result": None})
            return
        if method == "exit":
            sys.exit(0)
        if method == "textDocument/didOpen":
            td = params["textDocument"]
            self.analyze(td["uri"], td["text"])
            return
        if method == "textDocument/didChange":
            uri = params["textDocument"]["uri"]
            text = params["contentChanges"][-1]["text"]
            self.analyze(uri, text)
            return
        if method == "textDocument/didClose":
            uri = params["textDocument"]["uri"]
            opened = self.files.pop(uri, None)
            if opened and opened.doc is not None:
                opened.doc.close()
            return
        if method == "textDocument/hover":
            pos = params["position"]
            result = self.hover(params["textDocument"]["uri"], pos["line"], pos["character"])
            self.write({"jsonrpc": "2.0", "id": req_id, "result": result})
            return
        if method == "textDocument/definition":
            pos = params["position"]
            result = self.definition(
                params["textDocument"]["uri"], pos["line"], pos["character"]
            )
            self.write({"jsonrpc": "2.0", "id": req_id, "result": result})
            return
        if req_id is not None:
            self.write(
                {
                    "jsonrpc": "2.0",
                    "id": req_id,
                    "error": {"code": -32601, "message": f"method not found: {method}"},
                }
            )

    def serve(self) -> None:
        while True:
            msg = self.read()
            if msg is None:
                break
            self.handle(msg)


def main() -> None:
    Server(sys.stdin, sys.stdout).serve()


if __name__ == "__main__":
    main()
