package genlang;

import com.sun.jna.IntegerType;
import com.sun.jna.Library;
import com.sun.jna.Native;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.IntByReference;
import com.sun.jna.ptr.PointerByReference;

interface Lib extends Library {
    Lib INSTANCE = Native.load("genlang", Lib.class);

    final class SizeT extends IntegerType {
        public SizeT() {
            this(0);
        }

        public SizeT(long value) {
            super(Native.SIZE_T_SIZE, value, true);
        }
    }

    Pointer gen_version();

    Pointer gen_context_create();

    void gen_context_free(Pointer ctx);

    Pointer gen_context_last_error(Pointer ctx);

    int gen_error_code(Pointer error);

    Pointer gen_error_message(Pointer error);

    SizeT gen_error_line(Pointer error);

    SizeT gen_error_column(Pointer error);

    Pointer gen_error_path(Pointer error);

    int gen_document_parse(Pointer ctx, Pointer source, PointerByReference out);

    int gen_document_load_file(Pointer ctx, Pointer path, PointerByReference out);

    int gen_document_serialize(Pointer document, PointerByReference text, Pointer length);

    int gen_document_to_json(Pointer document, PointerByReference text, Pointer length);

    int gen_document_to_yaml(Pointer document, PointerByReference text, Pointer length);

    void gen_document_free(Pointer document);

    void gen_string_free(Pointer text);

    SizeT gen_type_count(Pointer document);

    int gen_type_name(Pointer document, SizeT index, PointerByReference name);

    int gen_type_parent(Pointer document, Pointer name, PointerByReference parent);

    SizeT gen_entity_count(Pointer document);

    int gen_entity_name(Pointer document, SizeT index, PointerByReference name);

    int gen_entity_type_name(Pointer document, Pointer name, PointerByReference type);

    int gen_entity_value(Pointer document, Pointer name, PointerByReference value);

    int gen_value_type(Pointer value);

    int gen_value_bool(Pointer value);

    long gen_value_int(Pointer value);

    double gen_value_float(Pointer value);

    Pointer gen_value_string(Pointer value);

    Pointer gen_value_reference(Pointer value);

    SizeT gen_value_list_count(Pointer value);

    int gen_value_list_get(Pointer value, SizeT index, PointerByReference item);

    SizeT gen_value_object_count(Pointer value);

    int gen_value_object_key(Pointer value, SizeT index, PointerByReference key);

    int gen_value_object_get_index(Pointer value, SizeT index, PointerByReference item);

    void gen_value_free(Pointer value);

    int gen_get(Pointer document, Pointer path, PointerByReference value);

    int gen_ancestors_of(Pointer document, Pointer name, PointerByReference result);

    int gen_set_members(Pointer document, Pointer setName, PointerByReference result);

    int gen_is_member(Pointer document, Pointer setName, Pointer entity, IntByReference flag);

    SizeT gen_query_result_count(Pointer result);

    Pointer gen_query_result_name(Pointer result, SizeT index);

    void gen_query_result_free(Pointer result);
}
