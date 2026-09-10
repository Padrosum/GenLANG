# GenLang grammar

Normative EBNF for GenLang 0.1.0. It matches the recursive-descent parser in `src/parser/parser.c`. Learning and usage: [The GenLang Book](book/genlang-book.md) (chapters 4–6). If this file and the book disagree on syntax, fix both to match the parser.

Lexical rules are applied before parsing. Whitespace and `#` line comments are discarded.

```ebnf
letter        = "A"…"Z" | "a"…"z" | "_" | utf8-ident ;
digit         = "0"…"9" ;
ident         = letter { letter | digit } ;

integer       = ["-"] digit { digit } ;
float         = ["-"] digit { digit } "." digit { digit } [ exponent ]
              | ["-"] digit { digit } exponent ;
exponent      = ("e" | "E") ["+" | "-"] digit { digit } ;

string        = '"' { string-char | escape } '"' ;
escape        = "\" ( '"' | "\" | "n" | "t" | "r" ) ;

boolean       = "true" | "false" ;
null          = "null" ;
```

Identifiers are UTF-8. ASCII names match `[A-Za-z_][A-Za-z0-9_]*`; non-ASCII letters such as `ı` or `ö` are also allowed. Language punctuation and whitespace are not identifier characters.
Keywords are reserved and are not valid identifiers:

```text
cins tur kume veri uye iceaktar
dyaz goster uyeler icerir ustler altlar yol ara liste
yardim temizle cikis
true false null
```

## Documents

A document file contains only declarations.

```ebnf
program       = { declaration } EOF ;

declaration   = genus-decl | species-decl | set-decl
              | data-decl | membership-decl | import-decl ;

genus-decl    = "cins" ident [ "->" ident ] [ object ] ;
species-decl  = "tur"  ident [ "->" ident ] [ object ] ;
set-decl      = "kume" ident [ object ] ;

data-decl     = "veri" ident [ ":" ident ] ( object | "=" expression ) ;
membership-decl = "uye" ident "->" ident ;
import-decl   = "iceaktar" string ;

expression    = object | list | literal | reference ;
literal       = string | integer | float | boolean | null ;
reference     = "@" ident ;

list          = "[" [ expression { "," expression } [ "," ] ] "]" ;
object        = "{" { ident "=" expression [ "," ] } "}" ;
```

Object properties may be separated by whitespace (including newlines) and optional commas.
List elements require commas. A trailing comma in a list is allowed.

`iceaktar "file.gl"` splices that file's declarations in place. Paths are resolved
relative to the importing file, must end with `.gl`, and are loaded once (diamond
imports are not duplicated). A cycle among files is `GEN_ERR_CYCLE`. In-memory
`gen_document_parse` rejects imports; use `gen_document_load_file` or
`gen_document_parse_at`. Imports never execute code, open network resources, or
run a shell. `gen_document_serialize` emits the merged document (no `iceaktar`
lines).

If a type declaration includes a property object, that object is an optional
schema: typed `veri` must provide those keys with the same value kinds.
Ancestor properties apply; a child type overrides the same key. Extra keys on
the instance are allowed. Types without properties are unconstrained.

## Paths (REPL / `gen_eval_path`)

```ebnf
path          = ident { "." ident | "[" integer "]" } ;
```

Negative indexes are rejected.

## REPL units

The REPL parser accepts a declaration, a command, or a path.

```ebnf
repl-unit     = declaration | command | path ;

command       = "dyaz" ident
              | "goster" ident
              | "uyeler" ident
              | "icerir" ident ident
              | "ustler" ident
              | "altlar" ident
              | "yol" ident ident
              | "ara" ( string | ident )
              | "liste" ( "cins" | "tur" | "kume" | "veri" | ident )
              | "yardim"
              | "temizle"
              | "cikis" ;
```

Commands are not legal in document files.
