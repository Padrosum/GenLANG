#!/usr/bin/env bash
# Rebuild The GenLang Book PDF from docs/book/genlang-book.md
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BOOK="$ROOT/docs/book"
HTML="$BOOK/genlang-book.html"
PDF="$BOOK/genlang-book.pdf"
CSS="$BOOK/book.css"
MD="$BOOK/genlang-book.md"
VENV="${GENLANG_BOOK_VENV:-$ROOT/build/book-venv}"

if ! command -v pandoc >/dev/null; then
    echo "pandoc is required" >&2
    exit 1
fi
if [[ ! -x "$VENV/bin/python" ]]; then
    python3 -m venv "$VENV"
    "$VENV/bin/pip" install -q --index-url https://pypi.org/simple weasyprint
fi

pandoc "$MD" \
    -s --toc --toc-depth=2 \
    --metadata title="The GenLang Book" \
    --css="$CSS" \
    -o "$HTML"

"$VENV/bin/python" - <<PY
from pathlib import Path
from weasyprint import HTML
html = Path("$HTML")
css = Path("$CSS")
HTML(filename=str(html), base_url=str(html.parent)).write_pdf(
    "$PDF", stylesheets=[str(css)]
)
print("wrote $PDF")
PY
