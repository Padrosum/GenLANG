using System.Runtime.InteropServices;

namespace GenLang;

public sealed class Ref
{
    public Ref(string name) => Name = name;
    public string Name { get; }
    public override string ToString() => "@" + Name;
}

public sealed class GenLangException : Exception
{
    public GenLangException(int code, string message, int line = 0, int column = 0, string? path = null)
        : base(line > 0 ? $"{path ?? "<input>"}:{line}:{column}: {message}" : message)
    {
        Code = code;
        Line = line;
        Column = column;
        Path = path;
    }

    public int Code { get; }
    public int Line { get; }
    public int Column { get; }
    public string? Path { get; }
}

public sealed class Document : IDisposable
{
    private IntPtr _ctx;
    private IntPtr _doc;

    private Document(IntPtr ctx, IntPtr doc)
    {
        _ctx = ctx;
        _doc = doc;
    }

    public static string Version() => Native.Utf8(Native.gen_version()) ?? "";

    public static Document Parse(string source) => Open(source, loadFile: false);

    public static Document Load(string path) => Open(path, loadFile: true);

    private static Document Open(string arg, bool loadFile)
    {
        var ctx = Native.gen_context_create();
        if (ctx == IntPtr.Zero)
        {
            throw new GenLangException(2, "out of memory");
        }
        var cArg = Native.Utf8Alloc(arg);
        try
        {
            var rc = loadFile
                ? Native.gen_document_load_file(ctx, cArg, out var doc)
                : Native.gen_document_parse(ctx, cArg, out doc);
            if (rc != 0)
            {
                var err = Native.gen_context_last_error(ctx);
                var ex = new GenLangException(
                    rc,
                    Native.Utf8(Native.gen_error_message(err)) ?? "error",
                    (int)Native.gen_error_line(err),
                    (int)Native.gen_error_column(err),
                    Native.Utf8(Native.gen_error_path(err))
                );
                Native.gen_context_free(ctx);
                throw ex;
            }
            return new Document(ctx, doc);
        }
        finally
        {
            Marshal.FreeHGlobal(cArg);
        }
    }

    public void Dispose()
    {
        if (_doc != IntPtr.Zero)
        {
            Native.gen_document_free(_doc);
            _doc = IntPtr.Zero;
        }
        if (_ctx != IntPtr.Zero)
        {
            Native.gen_context_free(_ctx);
            _ctx = IntPtr.Zero;
        }
        GC.SuppressFinalize(this);
    }

    ~Document() => Dispose();

    private void EnsureOpen()
    {
        if (_doc == IntPtr.Zero)
        {
            throw new ObjectDisposedException(nameof(Document));
        }
    }

    public IReadOnlyList<string> Types
    {
        get
        {
            EnsureOpen();
            var n = (int)Native.gen_type_count(_doc);
            var names = new List<string>(n);
            for (var i = 0; i < n; i++)
            {
                Native.gen_type_name(_doc, (UIntPtr)i, out var name);
                names.Add(Native.Utf8(name) ?? "");
            }
            return names;
        }
    }

    public IReadOnlyList<string> Entities
    {
        get
        {
            EnsureOpen();
            var n = (int)Native.gen_entity_count(_doc);
            var names = new List<string>(n);
            for (var i = 0; i < n; i++)
            {
                Native.gen_entity_name(_doc, (UIntPtr)i, out var name);
                names.Add(Native.Utf8(name) ?? "");
            }
            return names;
        }
    }

    public string? TypeParent(string name)
    {
        EnsureOpen();
        var cName = Native.Utf8Alloc(name);
        try
        {
            var rc = Native.gen_type_parent(_doc, cName, out var parent);
            if (rc != 0)
            {
                throw new GenLangException(rc, $"unknown type '{name}'");
            }
            return Native.Utf8(parent);
        }
        finally
        {
            Marshal.FreeHGlobal(cName);
        }
    }

    public string? EntityType(string name)
    {
        EnsureOpen();
        var cName = Native.Utf8Alloc(name);
        try
        {
            var rc = Native.gen_entity_type_name(_doc, cName, out var type);
            if (rc != 0)
            {
                throw new GenLangException(rc, $"unknown entity '{name}'");
            }
            return Native.Utf8(type);
        }
        finally
        {
            Marshal.FreeHGlobal(cName);
        }
    }

    public object? Entity(string name)
    {
        EnsureOpen();
        var cName = Native.Utf8Alloc(name);
        try
        {
            var rc = Native.gen_entity_value(_doc, cName, out var value);
            if (rc != 0)
            {
                throw new GenLangException(rc, $"unknown entity '{name}'");
            }
            return ValueFromNative(value);
        }
        finally
        {
            Marshal.FreeHGlobal(cName);
        }
    }

    public object? Get(string path)
    {
        EnsureOpen();
        var cPath = Native.Utf8Alloc(path);
        try
        {
            var rc = Native.gen_get(_doc, cPath, out var value);
            if (rc != 0)
            {
                throw new GenLangException(rc, $"cannot evaluate path '{path}'");
            }
            try
            {
                return ValueFromNative(value);
            }
            finally
            {
                Native.gen_value_free(value);
            }
        }
        finally
        {
            Marshal.FreeHGlobal(cPath);
        }
    }

    public IReadOnlyList<string> Ancestors(string name)
    {
        EnsureOpen();
        var cName = Native.Utf8Alloc(name);
        try
        {
            var rc = Native.gen_ancestors_of(_doc, cName, out var result);
            return NamesFromQuery(rc, result, $"unknown type '{name}'");
        }
        finally
        {
            Marshal.FreeHGlobal(cName);
        }
    }

    public IReadOnlyList<string> Members(string setName)
    {
        EnsureOpen();
        var cName = Native.Utf8Alloc(setName);
        try
        {
            var rc = Native.gen_set_members(_doc, cName, out var result);
            return NamesFromQuery(rc, result, $"unknown set '{setName}'");
        }
        finally
        {
            Marshal.FreeHGlobal(cName);
        }
    }

    public IReadOnlyList<string> EntitiesOf(string typeName)
    {
        EnsureOpen();
        var cName = Native.Utf8Alloc(typeName);
        try
        {
            var rc = Native.gen_entities_of(_doc, cName, out var result);
            return NamesFromQuery(rc, result, $"unknown type '{typeName}'");
        }
        finally
        {
            Marshal.FreeHGlobal(cName);
        }
    }

    public bool IsMember(string setName, string entity)
    {
        EnsureOpen();
        var cSet = Native.Utf8Alloc(setName);
        var cEnt = Native.Utf8Alloc(entity);
        try
        {
            var rc = Native.gen_is_member(_doc, cSet, cEnt, out var flag);
            if (rc != 0)
            {
                throw new GenLangException(rc, $"unknown set '{setName}'");
            }
            return flag != 0;
        }
        finally
        {
            Marshal.FreeHGlobal(cSet);
            Marshal.FreeHGlobal(cEnt);
        }
    }

    public string Serialize()
    {
        EnsureOpen();
        var rc = Native.gen_document_serialize(_doc, out var text, IntPtr.Zero);
        if (rc != 0)
        {
            throw new GenLangException(rc, "serialize failed");
        }
        try
        {
            return Native.Utf8(text) ?? "";
        }
        finally
        {
            Native.gen_string_free(text);
        }
    }

    public string ToJson()
    {
        EnsureOpen();
        var rc = Native.gen_document_to_json(_doc, out var text, IntPtr.Zero);
        if (rc != 0)
        {
            throw new GenLangException(rc, "json conversion failed");
        }
        try
        {
            return Native.Utf8(text) ?? "";
        }
        finally
        {
            Native.gen_string_free(text);
        }
    }

    public string ToYaml()
    {
        EnsureOpen();
        var rc = Native.gen_document_to_yaml(_doc, out var text, IntPtr.Zero);
        if (rc != 0)
        {
            throw new GenLangException(rc, "yaml conversion failed");
        }
        try
        {
            return Native.Utf8(text) ?? "";
        }
        finally
        {
            Native.gen_string_free(text);
        }
    }

    private static IReadOnlyList<string> NamesFromQuery(int rc, IntPtr result, string error)
    {
        if (rc != 0)
        {
            throw new GenLangException(rc, error);
        }
        var n = (int)Native.gen_query_result_count(result);
        var names = new List<string>(n);
        for (var i = 0; i < n; i++)
        {
            names.Add(Native.Utf8(Native.gen_query_result_name(result, (UIntPtr)i)) ?? "");
        }
        Native.gen_query_result_free(result);
        return names;
    }

    private static object? ValueFromNative(IntPtr value)
    {
        if (value == IntPtr.Zero)
        {
            return null;
        }
        switch (Native.gen_value_type(value))
        {
            case 0:
                return null;
            case 1:
                return Native.gen_value_bool(value) != 0;
            case 2:
                return Native.gen_value_int(value);
            case 3:
                return Native.gen_value_float(value);
            case 4:
                return Native.Utf8(Native.gen_value_string(value)) ?? "";
            case 5:
            {
                var n = (int)Native.gen_value_list_count(value);
                var list = new List<object?>(n);
                for (var i = 0; i < n; i++)
                {
                    Native.gen_value_list_get(value, (UIntPtr)i, out var item);
                    list.Add(ValueFromNative(item));
                }
                return list;
            }
            case 6:
            {
                var n = (int)Native.gen_value_object_count(value);
                var obj = new Dictionary<string, object?>(n);
                for (var i = 0; i < n; i++)
                {
                    Native.gen_value_object_key(value, (UIntPtr)i, out var key);
                    Native.gen_value_object_get_index(value, (UIntPtr)i, out var item);
                    obj[Native.Utf8(key) ?? ""] = ValueFromNative(item);
                }
                return obj;
            }
            case 7:
                return new Ref(Native.Utf8(Native.gen_value_reference(value)) ?? "");
            default:
                return null;
        }
    }
}
