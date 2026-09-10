using System.Runtime.InteropServices;
using System.Text;

namespace GenLang;

internal static class Native
{
    private const string Lib = "genlang";

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr gen_version();

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr gen_context_create();

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void gen_context_free(IntPtr ctx);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr gen_context_last_error(IntPtr ctx);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int gen_error_code(IntPtr error);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr gen_error_message(IntPtr error);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern UIntPtr gen_error_line(IntPtr error);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern UIntPtr gen_error_column(IntPtr error);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr gen_error_path(IntPtr error);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int gen_document_parse(IntPtr ctx, IntPtr source, out IntPtr document);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int gen_document_load_file(IntPtr ctx, IntPtr path, out IntPtr document);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int gen_document_serialize(IntPtr document, out IntPtr text, IntPtr length);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int gen_document_to_json(IntPtr document, out IntPtr text, IntPtr length);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int gen_document_to_yaml(IntPtr document, out IntPtr text, IntPtr length);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void gen_document_free(IntPtr document);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void gen_string_free(IntPtr text);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern UIntPtr gen_type_count(IntPtr document);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int gen_type_name(IntPtr document, UIntPtr index, out IntPtr name);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int gen_type_parent(IntPtr document, IntPtr name, out IntPtr parent);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern UIntPtr gen_entity_count(IntPtr document);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int gen_entity_name(IntPtr document, UIntPtr index, out IntPtr name);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int gen_entity_type_name(IntPtr document, IntPtr name, out IntPtr type);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int gen_entity_value(IntPtr document, IntPtr name, out IntPtr value);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int gen_value_type(IntPtr value);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int gen_value_bool(IntPtr value);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern long gen_value_int(IntPtr value);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern double gen_value_float(IntPtr value);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr gen_value_string(IntPtr value);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr gen_value_reference(IntPtr value);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern UIntPtr gen_value_list_count(IntPtr value);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int gen_value_list_get(IntPtr value, UIntPtr index, out IntPtr item);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern UIntPtr gen_value_object_count(IntPtr value);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int gen_value_object_key(IntPtr value, UIntPtr index, out IntPtr key);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int gen_value_object_get_index(IntPtr value, UIntPtr index, out IntPtr item);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void gen_value_free(IntPtr value);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int gen_get(IntPtr document, IntPtr path, out IntPtr value);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int gen_ancestors_of(IntPtr document, IntPtr name, out IntPtr result);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int gen_set_members(IntPtr document, IntPtr setName, out IntPtr result);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int gen_is_member(IntPtr document, IntPtr setName, IntPtr entity, out int flag);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern UIntPtr gen_query_result_count(IntPtr result);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr gen_query_result_name(IntPtr result, UIntPtr index);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void gen_query_result_free(IntPtr result);

    internal static string? Utf8(IntPtr ptr) => ptr == IntPtr.Zero ? null : Marshal.PtrToStringUTF8(ptr);

    internal static IntPtr Utf8Alloc(string s)
    {
        var bytes = Encoding.UTF8.GetBytes(s);
        var ptr = Marshal.AllocHGlobal(bytes.Length + 1);
        Marshal.Copy(bytes, 0, ptr, bytes.Length);
        Marshal.WriteByte(ptr, bytes.Length, 0);
        return ptr;
    }
}
