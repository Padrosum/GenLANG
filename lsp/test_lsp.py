#!/usr/bin/env python3

from __future__ import annotations

import json
import os
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LSP = ROOT / "lsp" / "genlang_lsp.py"


def rpc(method: str, params=None, req_id=1) -> bytes:
    payload: dict = {"jsonrpc": "2.0", "method": method}
    if req_id is not None:
        payload["id"] = req_id
    if params is not None:
        payload["params"] = params
    raw = json.dumps(payload).encode("utf-8")
    return f"Content-Length: {len(raw)}\r\n\r\n".encode("ascii") + raw


class Reader:
    def __init__(self, stream) -> None:
        self.stream = stream
        self.buf = b""

    def read(self) -> dict:
        while True:
            sep = self.buf.find(b"\r\n\r\n")
            if sep != -1:
                header = self.buf[:sep]
                length = int(header.decode("ascii").split(":")[1].strip())
                start = sep + 4
                if len(self.buf) >= start + length:
                    body = self.buf[start : start + length]
                    self.buf = self.buf[start + length :]
                    return json.loads(body.decode("utf-8"))
            chunk = self.stream.read(1)
            if not chunk:
                raise EOFError("LSP closed stdout")
            self.buf += chunk


class TestLsp(unittest.TestCase):
    def test_initialize_diagnostics_hover_definition(self) -> None:
        env = os.environ.copy()
        env["PYTHONPATH"] = str(ROOT / "bindings" / "python" / "src")
        env["GENLANG_LIB_DIR"] = str(ROOT / "build")
        proc = subprocess.Popen(
            [sys.executable, str(LSP)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=env,
        )
        self.addCleanup(proc.kill)
        src_bad = "cins Canli\ncins Hayvan -> Canli\nveri x : Missing { n = 1 }\n"
        src_good = "cins Canli\ncins Hayvan -> Canli\n"
        uri = "file:///tmp/sample.gl"
        assert proc.stdin is not None and proc.stdout is not None
        reader = Reader(proc.stdout)
        proc.stdin.write(
            rpc("initialize", {"capabilities": {}}, 1)
            + rpc("initialized", {}, None)
            + rpc(
                "textDocument/didOpen",
                {
                    "textDocument": {
                        "uri": uri,
                        "languageId": "genlang",
                        "version": 1,
                        "text": src_bad,
                    }
                },
                None,
            )
        )
        proc.stdin.flush()

        init = reader.read()
        self.assertEqual(init["id"], 1)
        self.assertTrue(init["result"]["capabilities"]["hoverProvider"])
        self.assertTrue(init["result"]["capabilities"]["definitionProvider"])
        diags = reader.read()
        self.assertEqual(diags["method"], "textDocument/publishDiagnostics")
        messages = [d["message"] for d in diags["params"]["diagnostics"]]
        self.assertTrue(any("Missing" in m for m in messages))

        proc.stdin.write(
            rpc(
                "textDocument/didChange",
                {
                    "textDocument": {"uri": uri},
                    "contentChanges": [{"text": src_good}],
                },
                None,
            )
        )
        proc.stdin.flush()
        diags2 = reader.read()
        self.assertEqual(diags2["params"]["diagnostics"], [])

        proc.stdin.write(
            rpc(
                "textDocument/hover",
                {"textDocument": {"uri": uri}, "position": {"line": 1, "character": 6}},
                2,
            )
        )
        proc.stdin.flush()
        hover_msg = reader.read()
        self.assertEqual(hover_msg["id"], 2)
        self.assertIn("Hayvan", hover_msg["result"]["contents"]["value"])

        proc.stdin.write(
            rpc(
                "textDocument/definition",
                {"textDocument": {"uri": uri}, "position": {"line": 1, "character": 6}},
                3,
            )
        )
        proc.stdin.flush()
        defn = reader.read()
        self.assertEqual(defn["id"], 3)
        self.assertEqual(defn["result"]["range"]["start"]["line"], 1)

        proc.stdin.write(rpc("shutdown", {}, 4))
        proc.stdin.write(rpc("exit", {}, None))
        proc.stdin.close()
        self.assertEqual(proc.wait(timeout=5), 0)


if __name__ == "__main__":
    unittest.main()
