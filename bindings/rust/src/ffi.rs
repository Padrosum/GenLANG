//! Raw FFI for libgenlang. Prefer the safe wrappers in `lib.rs`.

#![allow(dead_code, non_camel_case_types)]

use std::os::raw::{c_char, c_double, c_int};

pub type size_t = usize;

pub const GEN_OK: c_int = 0;
pub const GEN_ERR_INVALID_ARGUMENT: c_int = 1;
pub const GEN_ERR_OUT_OF_MEMORY: c_int = 2;
pub const GEN_ERR_IO: c_int = 3;
pub const GEN_ERR_LEX: c_int = 4;
pub const GEN_ERR_PARSE: c_int = 5;
pub const GEN_ERR_SEMANTIC: c_int = 6;
pub const GEN_ERR_NOT_FOUND: c_int = 7;
pub const GEN_ERR_TYPE: c_int = 8;
pub const GEN_ERR_INDEX: c_int = 9;
pub const GEN_ERR_DUPLICATE: c_int = 10;
pub const GEN_ERR_CYCLE: c_int = 11;
pub const GEN_ERR_INVALID_OPERATION: c_int = 12;
pub const GEN_ERR_SERIALIZATION: c_int = 13;
pub const GEN_ERR_QUERY: c_int = 14;

pub const GEN_VALUE_NULL: c_int = 0;
pub const GEN_VALUE_BOOL: c_int = 1;
pub const GEN_VALUE_INT: c_int = 2;
pub const GEN_VALUE_FLOAT: c_int = 3;
pub const GEN_VALUE_STRING: c_int = 4;
pub const GEN_VALUE_LIST: c_int = 5;
pub const GEN_VALUE_OBJECT: c_int = 6;
pub const GEN_VALUE_REFERENCE: c_int = 7;

#[repr(C)]
pub struct GenContext {
    _private: [u8; 0],
}

#[repr(C)]
pub struct GenDocument {
    _private: [u8; 0],
}

#[repr(C)]
pub struct GenValue {
    _private: [u8; 0],
}

#[repr(C)]
pub struct GenError {
    _private: [u8; 0],
}

#[repr(C)]
pub struct GenQueryResult {
    _private: [u8; 0],
}

unsafe extern "C" {
    pub fn gen_version() -> *const c_char;
    pub fn gen_context_create() -> *mut GenContext;
    pub fn gen_context_free(ctx: *mut GenContext);
    pub fn gen_context_last_error(ctx: *const GenContext) -> *const GenError;
    pub fn gen_error_code(error: *const GenError) -> c_int;
    pub fn gen_error_message(error: *const GenError) -> *const c_char;
    pub fn gen_error_line(error: *const GenError) -> size_t;
    pub fn gen_error_column(error: *const GenError) -> size_t;
    pub fn gen_error_path(error: *const GenError) -> *const c_char;
    pub fn gen_document_parse(
        ctx: *mut GenContext,
        source: *const c_char,
        out_document: *mut *mut GenDocument,
    ) -> c_int;
    pub fn gen_document_load_file(
        ctx: *mut GenContext,
        path: *const c_char,
        out_document: *mut *mut GenDocument,
    ) -> c_int;
    pub fn gen_document_serialize(
        document: *const GenDocument,
        out_text: *mut *mut c_char,
        out_length: *mut size_t,
    ) -> c_int;
    pub fn gen_document_to_json(
        document: *const GenDocument,
        out_text: *mut *mut c_char,
        out_length: *mut size_t,
    ) -> c_int;
    pub fn gen_document_to_yaml(
        document: *const GenDocument,
        out_text: *mut *mut c_char,
        out_length: *mut size_t,
    ) -> c_int;
    pub fn gen_document_free(document: *mut GenDocument);
    pub fn gen_string_free(string: *mut c_char);
    pub fn gen_type_count(document: *const GenDocument) -> size_t;
    pub fn gen_type_name(
        document: *const GenDocument,
        index: size_t,
        out_name: *mut *const c_char,
    ) -> c_int;
    pub fn gen_type_parent(
        document: *const GenDocument,
        name: *const c_char,
        out_parent: *mut *const c_char,
    ) -> c_int;
    pub fn gen_entity_count(document: *const GenDocument) -> size_t;
    pub fn gen_entity_name(
        document: *const GenDocument,
        index: size_t,
        out_name: *mut *const c_char,
    ) -> c_int;
    pub fn gen_entity_type_name(
        document: *const GenDocument,
        name: *const c_char,
        out_type: *mut *const c_char,
    ) -> c_int;
    pub fn gen_entity_value(
        document: *const GenDocument,
        name: *const c_char,
        out_value: *mut *const GenValue,
    ) -> c_int;
    pub fn gen_value_type(value: *const GenValue) -> c_int;
    pub fn gen_value_bool(value: *const GenValue) -> c_int;
    pub fn gen_value_int(value: *const GenValue) -> i64;
    pub fn gen_value_float(value: *const GenValue) -> c_double;
    pub fn gen_value_string(value: *const GenValue) -> *const c_char;
    pub fn gen_value_reference(value: *const GenValue) -> *const c_char;
    pub fn gen_value_list_count(value: *const GenValue) -> size_t;
    pub fn gen_value_list_get(
        value: *const GenValue,
        index: size_t,
        out_item: *mut *const GenValue,
    ) -> c_int;
    pub fn gen_value_object_count(value: *const GenValue) -> size_t;
    pub fn gen_value_object_key(
        value: *const GenValue,
        index: size_t,
        out_key: *mut *const c_char,
    ) -> c_int;
    pub fn gen_value_object_get_index(
        value: *const GenValue,
        index: size_t,
        out_item: *mut *const GenValue,
    ) -> c_int;
    pub fn gen_value_free(value: *mut GenValue);
    pub fn gen_get(
        document: *const GenDocument,
        path: *const c_char,
        out_value: *mut *mut GenValue,
    ) -> c_int;
    pub fn gen_ancestors_of(
        document: *const GenDocument,
        name: *const c_char,
        out_result: *mut *mut GenQueryResult,
    ) -> c_int;
    pub fn gen_memberships_of(
        document: *const GenDocument,
        name: *const c_char,
        out_result: *mut *mut GenQueryResult,
    ) -> c_int;
    pub fn gen_set_members(
        document: *const GenDocument,
        set_name: *const c_char,
        out_result: *mut *mut GenQueryResult,
    ) -> c_int;
    pub fn gen_entities_of(
        document: *const GenDocument,
        type_name: *const c_char,
        out_result: *mut *mut GenQueryResult,
    ) -> c_int;
    pub fn gen_is_member(
        document: *const GenDocument,
        set_name: *const c_char,
        entity_name: *const c_char,
        out_is_member: *mut c_int,
    ) -> c_int;
    pub fn gen_query_result_count(result: *const GenQueryResult) -> size_t;
    pub fn gen_query_result_name(result: *const GenQueryResult, index: size_t) -> *const c_char;
    pub fn gen_query_result_free(result: *mut GenQueryResult);
}
