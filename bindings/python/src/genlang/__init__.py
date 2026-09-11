"""Python bindings for libgenlang (cffi, ABI mode)."""

from __future__ import annotations

from ._ffi import ffi, lib

__all__ = [
    "Document",
    "GenLangError",
    "Ref",
    "load",
    "parse",
    "parse_at",
    "from_json",
    "from_yaml",
    "from_binary",
    "try_parse",
    "version",
]

_CODE_NAMES = {
    lib.GEN_OK: "ok",
    lib.GEN_ERR_INVALID_ARGUMENT: "invalid_argument",
    lib.GEN_ERR_OUT_OF_MEMORY: "out_of_memory",
    lib.GEN_ERR_IO: "io",
    lib.GEN_ERR_LEX: "lex",
    lib.GEN_ERR_PARSE: "parse",
    lib.GEN_ERR_SEMANTIC: "semantic",
    lib.GEN_ERR_NOT_FOUND: "not_found",
    lib.GEN_ERR_TYPE: "type",
    lib.GEN_ERR_INDEX: "index",
    lib.GEN_ERR_DUPLICATE: "duplicate",
    lib.GEN_ERR_CYCLE: "cycle",
    lib.GEN_ERR_INVALID_OPERATION: "invalid_operation",
    lib.GEN_ERR_SERIALIZATION: "serialization",
    lib.GEN_ERR_QUERY: "query",
}


class Ref:
    """A `@name` reference copied out of a GenLang value."""

    __slots__ = ("name",)

    def __init__(self, name: str) -> None:
        self.name = name

    def __repr__(self) -> str:
        return f"@{self.name}"

    def __eq__(self, other: object) -> bool:
        return isinstance(other, Ref) and other.name == self.name


class GenLangError(Exception):
    def __init__(
        self,
        code: int,
        message: str,
        line: int = 0,
        column: int = 0,
        path: str | None = None,
    ) -> None:
        self.code = code
        self.message = message
        self.line = line
        self.column = column
        self.path = path
        loc = f"{path or '<input>'}:{line}:{column}: " if line else ""
        super().__init__(f"{loc}{_CODE_NAMES.get(code, 'error')}: {message}")


def version() -> str:
    return ffi.string(lib.gen_version()).decode("utf-8")


def _cstr(ptr) -> str | None:
    if ptr == ffi.NULL or ptr is None:
        return None
    return ffi.string(ptr).decode("utf-8")


def _raise_from_ctx(ctx, rc: int) -> None:
    err = lib.gen_context_last_error(ctx)
    raise GenLangError(
        int(rc),
        _cstr(lib.gen_error_message(err)) or "",
        int(lib.gen_error_line(err)),
        int(lib.gen_error_column(err)),
        _cstr(lib.gen_error_path(err)),
    )


def _value_to_python(ptr) -> object:
    if ptr == ffi.NULL:
        return None
    kind = lib.gen_value_type(ptr)
    if kind == lib.GEN_VALUE_NULL:
        return None
    if kind == lib.GEN_VALUE_BOOL:
        return bool(lib.gen_value_bool(ptr))
    if kind == lib.GEN_VALUE_INT:
        return int(lib.gen_value_int(ptr))
    if kind == lib.GEN_VALUE_FLOAT:
        return float(lib.gen_value_float(ptr))
    if kind == lib.GEN_VALUE_STRING:
        return _cstr(lib.gen_value_string(ptr)) or ""
    if kind == lib.GEN_VALUE_REFERENCE:
        return Ref(_cstr(lib.gen_value_reference(ptr)) or "")
    if kind == lib.GEN_VALUE_LIST:
        items = []
        n = lib.gen_value_list_count(ptr)
        for i in range(n):
            item = ffi.new("const GenValue **")
            lib.gen_value_list_get(ptr, i, item)
            items.append(_value_to_python(item[0]))
        return items
    if kind == lib.GEN_VALUE_OBJECT:
        obj: dict[str, object] = {}
        n = lib.gen_value_object_count(ptr)
        for i in range(n):
            key_out = ffi.new("const char **")
            val_out = ffi.new("const GenValue **")
            lib.gen_value_object_key(ptr, i, key_out)
            lib.gen_value_object_get_index(ptr, i, val_out)
            obj[_cstr(key_out[0]) or ""] = _value_to_python(val_out[0])
        return obj
    return None


def _query_names(rc: int, result_ptr, ctx=None) -> list[str]:
    if rc != lib.GEN_OK:
        if ctx is not None:
            _raise_from_ctx(ctx, rc)
        raise GenLangError(int(rc), "query failed")
    result = result_ptr[0]
    names = [
        _cstr(lib.gen_query_result_name(result, i)) or ""
        for i in range(lib.gen_query_result_count(result))
    ]
    lib.gen_query_result_free(result)
    return names


class Document:
    """An owned GenLang document. Values are copied into Python objects."""

    def __init__(self, ctx, doc) -> None:
        self._ctx = ctx
        self._doc = doc

    def close(self) -> None:
        if getattr(self, "_doc", None) not in (None, ffi.NULL):
            lib.gen_document_free(self._doc)
            self._doc = None
        if getattr(self, "_ctx", None) not in (None, ffi.NULL):
            lib.gen_context_free(self._ctx)
            self._ctx = None

    def __enter__(self) -> Document:
        return self

    def __exit__(self, *exc: object) -> None:
        self.close()

    def __del__(self) -> None:
        self.close()

    @property
    def types(self) -> list[str]:
        names: list[str] = []
        n = lib.gen_type_count(self._doc)
        for i in range(n):
            out = ffi.new("const char **")
            lib.gen_type_name(self._doc, i, out)
            names.append(_cstr(out[0]) or "")
        return names

    @property
    def sets(self) -> list[str]:
        names: list[str] = []
        n = lib.gen_set_count(self._doc)
        for i in range(n):
            out = ffi.new("const char **")
            lib.gen_set_name(self._doc, i, out)
            names.append(_cstr(out[0]) or "")
        return names

    @property
    def entities(self) -> list[str]:
        names: list[str] = []
        n = lib.gen_entity_count(self._doc)
        for i in range(n):
            out = ffi.new("const char **")
            lib.gen_entity_name(self._doc, i, out)
            names.append(_cstr(out[0]) or "")
        return names

    def type_parent(self, name: str) -> str | None:
        out = ffi.new("const char **")
        rc = lib.gen_type_parent(self._doc, name.encode("utf-8"), out)
        if rc != lib.GEN_OK:
            raise GenLangError(int(rc), f"unknown type '{name}'")
        return _cstr(out[0])

    def _location(self, fn, name: str, kind: str) -> tuple[int, int]:
        line = ffi.new("size_t *")
        column = ffi.new("size_t *")
        rc = fn(self._doc, name.encode("utf-8"), line, column)
        if rc != lib.GEN_OK:
            raise GenLangError(int(rc), f"unknown {kind} '{name}'")
        return int(line[0]), int(column[0])

    def type_location(self, name: str) -> tuple[int, int]:
        return self._location(lib.gen_type_location, name, "type")

    def set_location(self, name: str) -> tuple[int, int]:
        return self._location(lib.gen_set_location, name, "set")

    def entity_location(self, name: str) -> tuple[int, int]:
        return self._location(lib.gen_entity_location, name, "entity")

    def entity_type(self, name: str) -> str | None:
        out = ffi.new("const char **")
        rc = lib.gen_entity_type_name(self._doc, name.encode("utf-8"), out)
        if rc != lib.GEN_OK:
            raise GenLangError(int(rc), f"unknown entity '{name}'")
        return _cstr(out[0])

    def entity(self, name: str) -> object:
        out = ffi.new("const GenValue **")
        rc = lib.gen_entity_value(self._doc, name.encode("utf-8"), out)
        if rc != lib.GEN_OK:
            raise GenLangError(int(rc), f"unknown entity '{name}'")
        return _value_to_python(out[0])

    def get(self, path: str) -> object:
        out = ffi.new("GenValue **")
        rc = lib.gen_get(self._doc, path.encode("utf-8"), out)
        if rc != lib.GEN_OK:
            raise GenLangError(int(rc), f"cannot evaluate path '{path}'")
        try:
            return _value_to_python(out[0])
        finally:
            lib.gen_value_free(out[0])

    def types_of(self, entity: str) -> list[str]:
        result = ffi.new("GenQueryResult **")
        return _query_names(
            lib.gen_types_of(self._doc, entity.encode("utf-8"), result), result
        )

    def ancestors(self, type_name: str) -> list[str]:
        result = ffi.new("GenQueryResult **")
        return _query_names(
            lib.gen_ancestors_of(self._doc, type_name.encode("utf-8"), result), result
        )

    def descendants(self, type_name: str) -> list[str]:
        result = ffi.new("GenQueryResult **")
        return _query_names(
            lib.gen_descendants_of(self._doc, type_name.encode("utf-8"), result), result
        )

    def memberships(self, entity: str) -> list[str]:
        result = ffi.new("GenQueryResult **")
        return _query_names(
            lib.gen_memberships_of(self._doc, entity.encode("utf-8"), result), result
        )

    def members(self, set_name: str) -> list[str]:
        result = ffi.new("GenQueryResult **")
        return _query_names(
            lib.gen_set_members(self._doc, set_name.encode("utf-8"), result), result
        )

    def entities_of(self, type_name: str) -> list[str]:
        result = ffi.new("GenQueryResult **")
        return _query_names(
            lib.gen_entities_of(self._doc, type_name.encode("utf-8"), result), result
        )

    def is_member(self, set_name: str, entity: str) -> bool:
        flag = ffi.new("int *")
        rc = lib.gen_is_member(
            self._doc, set_name.encode("utf-8"), entity.encode("utf-8"), flag
        )
        if rc != lib.GEN_OK:
            raise GenLangError(int(rc), f"unknown set '{set_name}'")
        return bool(flag[0])

    def serialize(self) -> str:
        text = ffi.new("char **")
        rc = lib.gen_document_serialize(self._doc, text, ffi.NULL)
        if rc != lib.GEN_OK:
            raise GenLangError(int(rc), "serialize failed")
        try:
            return _cstr(text[0]) or ""
        finally:
            lib.gen_string_free(text[0])

    def to_json(self) -> str:
        text = ffi.new("char **")
        rc = lib.gen_document_to_json(self._doc, text, ffi.NULL)
        if rc != lib.GEN_OK:
            raise GenLangError(int(rc), "json conversion failed")
        try:
            return _cstr(text[0]) or ""
        finally:
            lib.gen_string_free(text[0])

    def to_yaml(self) -> str:
        text = ffi.new("char **")
        rc = lib.gen_document_to_yaml(self._doc, text, ffi.NULL)
        if rc != lib.GEN_OK:
            raise GenLangError(int(rc), "yaml conversion failed")
        try:
            return _cstr(text[0]) or ""
        finally:
            lib.gen_string_free(text[0])

    def to_binary(self) -> bytes:
        buf = ffi.new("char **")
        length = ffi.new("size_t *")
        rc = lib.gen_document_to_binary(self._doc, buf, length)
        if rc != lib.GEN_OK:
            raise GenLangError(int(rc), "binary conversion failed")
        try:
            return ffi.buffer(buf[0], int(length[0]))[:]
        finally:
            lib.gen_string_free(buf[0])


def parse(source: str) -> Document:
    ctx = lib.gen_context_create()
    if ctx == ffi.NULL:
        raise MemoryError("gen_context_create")
    doc = ffi.new("GenDocument **")
    rc = lib.gen_document_parse(ctx, source.encode("utf-8"), doc)
    if rc != lib.GEN_OK:
        try:
            _raise_from_ctx(ctx, rc)
        finally:
            lib.gen_context_free(ctx)
    return Document(ctx, doc[0])


def parse_at(source: str, origin_path: str) -> Document:
    ctx = lib.gen_context_create()
    if ctx == ffi.NULL:
        raise MemoryError("gen_context_create")
    doc = ffi.new("GenDocument **")
    rc = lib.gen_document_parse_at(
        ctx, source.encode("utf-8"), origin_path.encode("utf-8"), doc
    )
    if rc != lib.GEN_OK:
        try:
            _raise_from_ctx(ctx, rc)
        finally:
            lib.gen_context_free(ctx)
    return Document(ctx, doc[0])


def _errors_from_ctx(ctx) -> list[GenLangError]:
    n = int(lib.gen_context_error_count(ctx))
    if n == 0:
        err = lib.gen_context_last_error(ctx)
        code = int(lib.gen_error_code(err))
        if code == lib.GEN_OK:
            return []
        return [
            GenLangError(
                code,
                _cstr(lib.gen_error_message(err)) or "",
                int(lib.gen_error_line(err)),
                int(lib.gen_error_column(err)),
                _cstr(lib.gen_error_path(err)),
            )
        ]
    out: list[GenLangError] = []
    for i in range(n):
        err = lib.gen_context_error(ctx, i)
        out.append(
            GenLangError(
                int(lib.gen_error_code(err)),
                _cstr(lib.gen_error_message(err)) or "",
                int(lib.gen_error_line(err)),
                int(lib.gen_error_column(err)),
                _cstr(lib.gen_error_path(err)),
            )
        )
    return out


def try_parse(
    source: str, origin_path: str | None = None
) -> tuple[Document | None, list[GenLangError]]:
    """Parse without raising; used by the language server."""
    ctx = lib.gen_context_create()
    if ctx == ffi.NULL:
        raise MemoryError("gen_context_create")
    doc = ffi.new("GenDocument **")
    if origin_path:
        rc = lib.gen_document_parse_at(
            ctx, source.encode("utf-8"), origin_path.encode("utf-8"), doc
        )
    else:
        rc = lib.gen_document_parse(ctx, source.encode("utf-8"), doc)
    errors = _errors_from_ctx(ctx)
    if rc != lib.GEN_OK:
        lib.gen_document_free(doc[0])
        lib.gen_context_free(ctx)
        return None, errors
    return Document(ctx, doc[0]), errors


def load(path: str) -> Document:
    ctx = lib.gen_context_create()
    if ctx == ffi.NULL:
        raise MemoryError("gen_context_create")
    doc = ffi.new("GenDocument **")
    rc = lib.gen_document_load_file(ctx, path.encode("utf-8"), doc)
    if rc != lib.GEN_OK:
        try:
            _raise_from_ctx(ctx, rc)
        finally:
            lib.gen_context_free(ctx)
    return Document(ctx, doc[0])


def from_json(source: str) -> Document:
    ctx = lib.gen_context_create()
    if ctx == ffi.NULL:
        raise MemoryError("gen_context_create")
    doc = ffi.new("GenDocument **")
    rc = lib.gen_document_from_json(ctx, source.encode("utf-8"), doc)
    if rc != lib.GEN_OK:
        try:
            _raise_from_ctx(ctx, rc)
        finally:
            lib.gen_context_free(ctx)
    return Document(ctx, doc[0])


def from_yaml(source: str) -> Document:
    ctx = lib.gen_context_create()
    if ctx == ffi.NULL:
        raise MemoryError("gen_context_create")
    doc = ffi.new("GenDocument **")
    rc = lib.gen_document_from_yaml(ctx, source.encode("utf-8"), doc)
    if rc != lib.GEN_OK:
        try:
            _raise_from_ctx(ctx, rc)
        finally:
            lib.gen_context_free(ctx)
    return Document(ctx, doc[0])


def from_binary(data: bytes) -> Document:
    ctx = lib.gen_context_create()
    if ctx == ffi.NULL:
        raise MemoryError("gen_context_create")
    doc = ffi.new("GenDocument **")
    rc = lib.gen_document_from_binary(ctx, data, len(data), doc)
    if rc != lib.GEN_OK:
        try:
            _raise_from_ctx(ctx, rc)
        finally:
            lib.gen_context_free(ctx)
    return Document(ctx, doc[0])
