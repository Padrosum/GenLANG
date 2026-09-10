package genlang;

import com.sun.jna.Memory;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.IntByReference;
import com.sun.jna.ptr.PointerByReference;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public final class Document implements AutoCloseable {
    private Pointer ctx;
    private Pointer doc;

    private Document(Pointer ctx, Pointer doc) {
        this.ctx = ctx;
        this.doc = doc;
    }

    public static String version() {
        return utf8(Lib.INSTANCE.gen_version());
    }

    public static Document parse(String source) {
        return open(source, false);
    }

    public static Document load(String path) {
        return open(path, true);
    }

    private static Document open(String arg, boolean loadFile) {
        Pointer ctx = Lib.INSTANCE.gen_context_create();
        if (ctx == null) {
            throw new GenLangException(2, "out of memory");
        }
        Memory cArg = utf8z(arg);
        PointerByReference out = new PointerByReference();
        int rc = loadFile
                ? Lib.INSTANCE.gen_document_load_file(ctx, cArg, out)
                : Lib.INSTANCE.gen_document_parse(ctx, cArg, out);
        if (rc != 0) {
            Pointer err = Lib.INSTANCE.gen_context_last_error(ctx);
            GenLangException ex = new GenLangException(
                    rc,
                    utf8(Lib.INSTANCE.gen_error_message(err)),
                    (int) Lib.INSTANCE.gen_error_line(err).longValue(),
                    (int) Lib.INSTANCE.gen_error_column(err).longValue(),
                    utf8(Lib.INSTANCE.gen_error_path(err))
            );
            Lib.INSTANCE.gen_context_free(ctx);
            throw ex;
        }
        return new Document(ctx, out.getValue());
    }

    @Override
    public void close() {
        if (doc != null) {
            Lib.INSTANCE.gen_document_free(doc);
            doc = null;
        }
        if (ctx != null) {
            Lib.INSTANCE.gen_context_free(ctx);
            ctx = null;
        }
    }

    private void ensureOpen() {
        if (doc == null) {
            throw new IllegalStateException("document is closed");
        }
    }

    public List<String> types() {
        ensureOpen();
        int n = (int) Lib.INSTANCE.gen_type_count(doc).longValue();
        List<String> names = new ArrayList<>(n);
        for (int i = 0; i < n; i++) {
            PointerByReference name = new PointerByReference();
            Lib.INSTANCE.gen_type_name(doc, new Lib.SizeT(i), name);
            names.add(utf8(name.getValue()));
        }
        return names;
    }

    public List<String> entities() {
        ensureOpen();
        int n = (int) Lib.INSTANCE.gen_entity_count(doc).longValue();
        List<String> names = new ArrayList<>(n);
        for (int i = 0; i < n; i++) {
            PointerByReference name = new PointerByReference();
            Lib.INSTANCE.gen_entity_name(doc, new Lib.SizeT(i), name);
            names.add(utf8(name.getValue()));
        }
        return names;
    }

    public String typeParent(String name) {
        ensureOpen();
        PointerByReference parent = new PointerByReference();
        int rc = Lib.INSTANCE.gen_type_parent(doc, utf8z(name), parent);
        if (rc != 0) {
            throw new GenLangException(rc, "unknown type '" + name + "'");
        }
        return utf8(parent.getValue());
    }

    public String entityType(String name) {
        ensureOpen();
        PointerByReference type = new PointerByReference();
        int rc = Lib.INSTANCE.gen_entity_type_name(doc, utf8z(name), type);
        if (rc != 0) {
            throw new GenLangException(rc, "unknown entity '" + name + "'");
        }
        return utf8(type.getValue());
    }

    public Object entity(String name) {
        ensureOpen();
        PointerByReference value = new PointerByReference();
        int rc = Lib.INSTANCE.gen_entity_value(doc, utf8z(name), value);
        if (rc != 0) {
            throw new GenLangException(rc, "unknown entity '" + name + "'");
        }
        return valueFromNative(value.getValue());
    }

    public Object get(String path) {
        ensureOpen();
        PointerByReference value = new PointerByReference();
        int rc = Lib.INSTANCE.gen_get(doc, utf8z(path), value);
        if (rc != 0) {
            throw new GenLangException(rc, "cannot evaluate path '" + path + "'");
        }
        try {
            return valueFromNative(value.getValue());
        } finally {
            Lib.INSTANCE.gen_value_free(value.getValue());
        }
    }

    public List<String> ancestors(String name) {
        ensureOpen();
        PointerByReference result = new PointerByReference();
        int rc = Lib.INSTANCE.gen_ancestors_of(doc, utf8z(name), result);
        return namesFromQuery(rc, result.getValue(), "unknown type '" + name + "'");
    }

    public List<String> members(String setName) {
        ensureOpen();
        PointerByReference result = new PointerByReference();
        int rc = Lib.INSTANCE.gen_set_members(doc, utf8z(setName), result);
        return namesFromQuery(rc, result.getValue(), "unknown set '" + setName + "'");
    }

    public boolean isMember(String setName, String entity) {
        ensureOpen();
        IntByReference flag = new IntByReference();
        int rc = Lib.INSTANCE.gen_is_member(doc, utf8z(setName), utf8z(entity), flag);
        if (rc != 0) {
            throw new GenLangException(rc, "unknown set '" + setName + "'");
        }
        return flag.getValue() != 0;
    }

    public String serialize() {
        ensureOpen();
        PointerByReference text = new PointerByReference();
        int rc = Lib.INSTANCE.gen_document_serialize(doc, text, null);
        if (rc != 0) {
            throw new GenLangException(rc, "serialize failed");
        }
        try {
            return utf8(text.getValue());
        } finally {
            Lib.INSTANCE.gen_string_free(text.getValue());
        }
    }

    public String toJson() {
        ensureOpen();
        PointerByReference text = new PointerByReference();
        int rc = Lib.INSTANCE.gen_document_to_json(doc, text, null);
        if (rc != 0) {
            throw new GenLangException(rc, "json conversion failed");
        }
        try {
            return utf8(text.getValue());
        } finally {
            Lib.INSTANCE.gen_string_free(text.getValue());
        }
    }

    public String toYaml() {
        ensureOpen();
        PointerByReference text = new PointerByReference();
        int rc = Lib.INSTANCE.gen_document_to_yaml(doc, text, null);
        if (rc != 0) {
            throw new GenLangException(rc, "yaml conversion failed");
        }
        try {
            return utf8(text.getValue());
        } finally {
            Lib.INSTANCE.gen_string_free(text.getValue());
        }
    }

    private static List<String> namesFromQuery(int rc, Pointer result, String error) {
        if (rc != 0) {
            throw new GenLangException(rc, error);
        }
        int n = (int) Lib.INSTANCE.gen_query_result_count(result).longValue();
        List<String> names = new ArrayList<>(n);
        for (int i = 0; i < n; i++) {
            names.add(utf8(Lib.INSTANCE.gen_query_result_name(result, new Lib.SizeT(i))));
        }
        Lib.INSTANCE.gen_query_result_free(result);
        return names;
    }

    private static Object valueFromNative(Pointer value) {
        if (value == null) {
            return null;
        }
        switch (Lib.INSTANCE.gen_value_type(value)) {
            case 0:
                return null;
            case 1:
                return Lib.INSTANCE.gen_value_bool(value) != 0;
            case 2:
                return Lib.INSTANCE.gen_value_int(value);
            case 3:
                return Lib.INSTANCE.gen_value_float(value);
            case 4:
                return utf8(Lib.INSTANCE.gen_value_string(value));
            case 5: {
                int n = (int) Lib.INSTANCE.gen_value_list_count(value).longValue();
                List<Object> list = new ArrayList<>(n);
                for (int i = 0; i < n; i++) {
                    PointerByReference item = new PointerByReference();
                    Lib.INSTANCE.gen_value_list_get(value, new Lib.SizeT(i), item);
                    list.add(valueFromNative(item.getValue()));
                }
                return list;
            }
            case 6: {
                int n = (int) Lib.INSTANCE.gen_value_object_count(value).longValue();
                Map<String, Object> obj = new LinkedHashMap<>();
                for (int i = 0; i < n; i++) {
                    PointerByReference key = new PointerByReference();
                    PointerByReference item = new PointerByReference();
                    Lib.INSTANCE.gen_value_object_key(value, new Lib.SizeT(i), key);
                    Lib.INSTANCE.gen_value_object_get_index(value, new Lib.SizeT(i), item);
                    obj.put(utf8(key.getValue()), valueFromNative(item.getValue()));
                }
                return obj;
            }
            case 7:
                return new Ref(utf8(Lib.INSTANCE.gen_value_reference(value)));
            default:
                return null;
        }
    }

    private static Memory utf8z(String s) {
        byte[] bytes = s.getBytes(StandardCharsets.UTF_8);
        Memory mem = new Memory(bytes.length + 1L);
        mem.write(0, bytes, 0, bytes.length);
        mem.setByte(bytes.length, (byte) 0);
        return mem;
    }

    private static String utf8(Pointer p) {
        if (p == null) {
            return null;
        }
        return p.getString(0, "UTF-8");
    }
}
