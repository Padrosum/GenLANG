# The GenLang Book

Canonical learning and usage text for GenLang 0.1.0.

| Format | File |
| --- | --- |
| Source | [genlang-book.md](genlang-book.md) |
| PDF | [genlang-book.pdf](genlang-book.pdf) |

If this book disagrees with any other markdown file, **this book wins**, and the other file should be corrected to match the parser and `include/genlang.h`.

Rebuild the PDF (`pandoc` + WeasyPrint in `build/book-venv`):

```bash
bash docs/book/build-pdf.sh
```
