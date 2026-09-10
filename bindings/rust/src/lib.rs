//! Safe wrappers around the libgenlang C ABI.

mod ffi;

use std::ffi::{CStr, CString};
use std::fmt;
use std::os::raw::c_int;
use std::ptr;

#[derive(Debug, Clone, PartialEq)]
pub enum Value {
    Null,
    Bool(bool),
    Int(i64),
    Float(f64),
    String(String),
    List(Vec<Value>),
    Object(Vec<(String, Value)>),
    Reference(String),
}

#[derive(Debug, Clone)]
pub struct Error {
    pub code: i32,
    pub message: String,
    pub line: usize,
    pub column: usize,
    pub path: Option<String>,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.line > 0 {
            write!(
                f,
                "{}:{}:{}: {}",
                self.path.as_deref().unwrap_or("<input>"),
                self.line,
                self.column,
                self.message
            )
        } else {
            write!(f, "{}", self.message)
        }
    }
}

impl std::error::Error for Error {}

fn cstring(s: &str) -> Result<CString, Error> {
    CString::new(s).map_err(|_| Error {
        code: ffi::GEN_ERR_INVALID_ARGUMENT,
        message: "string contains NUL".into(),
        line: 0,
        column: 0,
        path: None,
    })
}

fn cstr_to_string(ptr: *const std::os::raw::c_char) -> Option<String> {
    if ptr.is_null() {
        return None;
    }
    unsafe { CStr::from_ptr(ptr).to_str().ok().map(str::to_owned) }
}

unsafe fn error_from_ctx(ctx: *mut ffi::GenContext, code: c_int) -> Error {
    let err = ffi::gen_context_last_error(ctx);
    Error {
        code,
        message: cstr_to_string(ffi::gen_error_message(err)).unwrap_or_default(),
        line: ffi::gen_error_line(err),
        column: ffi::gen_error_column(err),
        path: cstr_to_string(ffi::gen_error_path(err)),
    }
}

unsafe fn value_from_ptr(ptr: *const ffi::GenValue) -> Value {
    if ptr.is_null() {
        return Value::Null;
    }
    match ffi::gen_value_type(ptr) {
        ffi::GEN_VALUE_NULL => Value::Null,
        ffi::GEN_VALUE_BOOL => Value::Bool(ffi::gen_value_bool(ptr) != 0),
        ffi::GEN_VALUE_INT => Value::Int(ffi::gen_value_int(ptr)),
        ffi::GEN_VALUE_FLOAT => Value::Float(ffi::gen_value_float(ptr)),
        ffi::GEN_VALUE_STRING => {
            Value::String(cstr_to_string(ffi::gen_value_string(ptr)).unwrap_or_default())
        }
        ffi::GEN_VALUE_REFERENCE => {
            Value::Reference(cstr_to_string(ffi::gen_value_reference(ptr)).unwrap_or_default())
        }
        ffi::GEN_VALUE_LIST => {
            let n = ffi::gen_value_list_count(ptr);
            let mut items = Vec::with_capacity(n);
            for i in 0..n {
                let mut item: *const ffi::GenValue = ptr::null();
                let _ = ffi::gen_value_list_get(ptr, i, &mut item);
                items.push(value_from_ptr(item));
            }
            Value::List(items)
        }
        ffi::GEN_VALUE_OBJECT => {
            let n = ffi::gen_value_object_count(ptr);
            let mut obj = Vec::with_capacity(n);
            for i in 0..n {
                let mut key: *const std::os::raw::c_char = ptr::null();
                let mut val: *const ffi::GenValue = ptr::null();
                let _ = ffi::gen_value_object_key(ptr, i, &mut key);
                let _ = ffi::gen_value_object_get_index(ptr, i, &mut val);
                obj.push((cstr_to_string(key).unwrap_or_default(), value_from_ptr(val)));
            }
            Value::Object(obj)
        }
        _ => Value::Null,
    }
}

unsafe fn names_from_query(rc: c_int, result: *mut ffi::GenQueryResult) -> Result<Vec<String>, i32> {
    if rc != ffi::GEN_OK {
        return Err(rc);
    }
    let n = ffi::gen_query_result_count(result);
    let mut names = Vec::with_capacity(n);
    for i in 0..n {
        names.push(cstr_to_string(ffi::gen_query_result_name(result, i)).unwrap_or_default());
    }
    ffi::gen_query_result_free(result);
    Ok(names)
}

pub fn version() -> String {
    unsafe { cstr_to_string(ffi::gen_version()).unwrap_or_default() }
}

pub struct Document {
    ctx: *mut ffi::GenContext,
    doc: *mut ffi::GenDocument,
}

unsafe impl Send for Document {}

impl Drop for Document {
    fn drop(&mut self) {
        unsafe {
            if !self.doc.is_null() {
                ffi::gen_document_free(self.doc);
                self.doc = ptr::null_mut();
            }
            if !self.ctx.is_null() {
                ffi::gen_context_free(self.ctx);
                self.ctx = ptr::null_mut();
            }
        }
    }
}

impl Document {
    pub fn parse(source: &str) -> Result<Self, Error> {
        let c_src = cstring(source)?;
        unsafe {
            let ctx = ffi::gen_context_create();
            if ctx.is_null() {
                return Err(Error {
                    code: ffi::GEN_ERR_OUT_OF_MEMORY,
                    message: "out of memory".into(),
                    line: 0,
                    column: 0,
                    path: None,
                });
            }
            let mut doc: *mut ffi::GenDocument = ptr::null_mut();
            let rc = ffi::gen_document_parse(ctx, c_src.as_ptr(), &mut doc);
            if rc != ffi::GEN_OK {
                let err = error_from_ctx(ctx, rc);
                ffi::gen_context_free(ctx);
                return Err(err);
            }
            Ok(Document { ctx, doc })
        }
    }

    pub fn load(path: &str) -> Result<Self, Error> {
        let c_path = cstring(path)?;
        unsafe {
            let ctx = ffi::gen_context_create();
            if ctx.is_null() {
                return Err(Error {
                    code: ffi::GEN_ERR_OUT_OF_MEMORY,
                    message: "out of memory".into(),
                    line: 0,
                    column: 0,
                    path: None,
                });
            }
            let mut doc: *mut ffi::GenDocument = ptr::null_mut();
            let rc = ffi::gen_document_load_file(ctx, c_path.as_ptr(), &mut doc);
            if rc != ffi::GEN_OK {
                let err = error_from_ctx(ctx, rc);
                ffi::gen_context_free(ctx);
                return Err(err);
            }
            Ok(Document { ctx, doc })
        }
    }

    fn listed(
        &self,
        count: unsafe extern "C" fn(*const ffi::GenDocument) -> ffi::size_t,
        name_fn: unsafe extern "C" fn(
            *const ffi::GenDocument,
            ffi::size_t,
            *mut *const std::os::raw::c_char,
        ) -> c_int,
    ) -> Vec<String> {
        unsafe {
            let n = count(self.doc);
            let mut names = Vec::with_capacity(n);
            for i in 0..n {
                let mut name: *const std::os::raw::c_char = ptr::null();
                let _ = name_fn(self.doc, i, &mut name);
                names.push(cstr_to_string(name).unwrap_or_default());
            }
            names
        }
    }

    pub fn types(&self) -> Vec<String> {
        self.listed(ffi::gen_type_count, ffi::gen_type_name)
    }

    pub fn entities(&self) -> Vec<String> {
        self.listed(ffi::gen_entity_count, ffi::gen_entity_name)
    }

    pub fn type_parent(&self, name: &str) -> Result<Option<String>, Error> {
        let c_name = cstring(name)?;
        unsafe {
            let mut parent: *const std::os::raw::c_char = ptr::null();
            let rc = ffi::gen_type_parent(self.doc, c_name.as_ptr(), &mut parent);
            if rc != ffi::GEN_OK {
                return Err(Error {
                    code: rc,
                    message: format!("unknown type '{name}'"),
                    line: 0,
                    column: 0,
                    path: None,
                });
            }
            Ok(cstr_to_string(parent))
        }
    }

    pub fn entity_type(&self, name: &str) -> Result<Option<String>, Error> {
        let c_name = cstring(name)?;
        unsafe {
            let mut ty: *const std::os::raw::c_char = ptr::null();
            let rc = ffi::gen_entity_type_name(self.doc, c_name.as_ptr(), &mut ty);
            if rc != ffi::GEN_OK {
                return Err(Error {
                    code: rc,
                    message: format!("unknown entity '{name}'"),
                    line: 0,
                    column: 0,
                    path: None,
                });
            }
            Ok(cstr_to_string(ty))
        }
    }

    pub fn entity(&self, name: &str) -> Result<Value, Error> {
        let c_name = cstring(name)?;
        unsafe {
            let mut value: *const ffi::GenValue = ptr::null();
            let rc = ffi::gen_entity_value(self.doc, c_name.as_ptr(), &mut value);
            if rc != ffi::GEN_OK {
                return Err(Error {
                    code: rc,
                    message: format!("unknown entity '{name}'"),
                    line: 0,
                    column: 0,
                    path: None,
                });
            }
            Ok(value_from_ptr(value))
        }
    }

    pub fn get(&self, path: &str) -> Result<Value, Error> {
        let c_path = cstring(path)?;
        unsafe {
            let mut value: *mut ffi::GenValue = ptr::null_mut();
            let rc = ffi::gen_get(self.doc, c_path.as_ptr(), &mut value);
            if rc != ffi::GEN_OK {
                return Err(Error {
                    code: rc,
                    message: format!("cannot evaluate path '{path}'"),
                    line: 0,
                    column: 0,
                    path: None,
                });
            }
            let owned = value_from_ptr(value);
            ffi::gen_value_free(value);
            Ok(owned)
        }
    }

    pub fn ancestors(&self, name: &str) -> Result<Vec<String>, Error> {
        let c_name = cstring(name)?;
        unsafe {
            let mut result: *mut ffi::GenQueryResult = ptr::null_mut();
            let rc = ffi::gen_ancestors_of(self.doc, c_name.as_ptr(), &mut result);
            names_from_query(rc, result).map_err(|code| Error {
                code,
                message: format!("unknown type '{name}'"),
                line: 0,
                column: 0,
                path: None,
            })
        }
    }

    pub fn memberships(&self, name: &str) -> Result<Vec<String>, Error> {
        let c_name = cstring(name)?;
        unsafe {
            let mut result: *mut ffi::GenQueryResult = ptr::null_mut();
            let rc = ffi::gen_memberships_of(self.doc, c_name.as_ptr(), &mut result);
            names_from_query(rc, result).map_err(|code| Error {
                code,
                message: format!("unknown entity '{name}'"),
                line: 0,
                column: 0,
                path: None,
            })
        }
    }

    pub fn members(&self, set_name: &str) -> Result<Vec<String>, Error> {
        let c_name = cstring(set_name)?;
        unsafe {
            let mut result: *mut ffi::GenQueryResult = ptr::null_mut();
            let rc = ffi::gen_set_members(self.doc, c_name.as_ptr(), &mut result);
            names_from_query(rc, result).map_err(|code| Error {
                code,
                message: format!("unknown set '{set_name}'"),
                line: 0,
                column: 0,
                path: None,
            })
        }
    }

    pub fn is_member(&self, set_name: &str, entity: &str) -> Result<bool, Error> {
        let c_set = cstring(set_name)?;
        let c_ent = cstring(entity)?;
        unsafe {
            let mut flag: c_int = 0;
            let rc = ffi::gen_is_member(self.doc, c_set.as_ptr(), c_ent.as_ptr(), &mut flag);
            if rc != ffi::GEN_OK {
                return Err(Error {
                    code: rc,
                    message: format!("unknown set '{set_name}'"),
                    line: 0,
                    column: 0,
                    path: None,
                });
            }
            Ok(flag != 0)
        }
    }

    pub fn serialize(&self) -> Result<String, Error> {
        unsafe {
            let mut text: *mut std::os::raw::c_char = ptr::null_mut();
            let rc = ffi::gen_document_serialize(self.doc, &mut text, ptr::null_mut());
            if rc != ffi::GEN_OK {
                return Err(Error {
                    code: rc,
                    message: "serialize failed".into(),
                    line: 0,
                    column: 0,
                    path: None,
                });
            }
            let s = cstr_to_string(text).unwrap_or_default();
            ffi::gen_string_free(text);
            Ok(s)
        }
    }

    pub fn to_json(&self) -> Result<String, Error> {
        unsafe {
            let mut text: *mut std::os::raw::c_char = ptr::null_mut();
            let rc = ffi::gen_document_to_json(self.doc, &mut text, ptr::null_mut());
            if rc != ffi::GEN_OK {
                return Err(Error {
                    code: rc,
                    message: "json conversion failed".into(),
                    line: 0,
                    column: 0,
                    path: None,
                });
            }
            let s = cstr_to_string(text).unwrap_or_default();
            ffi::gen_string_free(text);
            Ok(s)
        }
    }

    pub fn to_yaml(&self) -> Result<String, Error> {
        unsafe {
            let mut text: *mut std::os::raw::c_char = ptr::null_mut();
            let rc = ffi::gen_document_to_yaml(self.doc, &mut text, ptr::null_mut());
            if rc != ffi::GEN_OK {
                return Err(Error {
                    code: rc,
                    message: "yaml conversion failed".into(),
                    line: 0,
                    column: 0,
                    path: None,
                });
            }
            let s = cstr_to_string(text).unwrap_or_default();
            ffi::gen_string_free(text);
            Ok(s)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const SRC: &str = r#"
cins Canli
cins Hayvan -> Canli
tur Kedi -> Hayvan
kume Evcil
veri boncuk : Kedi {
    isim = "Boncuk"
    yas = 4
}
uye boncuk -> Evcil
"#;

    #[test]
    fn version_string() {
        assert_eq!(version(), "0.1.0");
    }

    #[test]
    fn parse_and_query() {
        let doc = Document::parse(SRC).unwrap();
        assert_eq!(doc.types(), ["Canli", "Hayvan", "Kedi"]);
        assert_eq!(doc.type_parent("Hayvan").unwrap(), Some("Canli".into()));
        assert_eq!(doc.ancestors("Kedi").unwrap(), ["Hayvan", "Canli"]);
        assert_eq!(doc.entity_type("boncuk").unwrap(), Some("Kedi".into()));
        assert!(doc.is_member("Evcil", "boncuk").unwrap());
        assert_eq!(doc.members("Evcil").unwrap(), ["boncuk"]);
        match doc.entity("boncuk").unwrap() {
            Value::Object(pairs) => {
                let yas = pairs.iter().find(|(k, _)| k == "yas").unwrap();
                assert_eq!(yas.1, Value::Int(4));
            }
            other => panic!("unexpected {other:?}"),
        }
        assert!(doc.serialize().unwrap().contains("cins Canli"));
        assert!(doc.to_json().unwrap().contains("boncuk"));
        assert!(doc.to_yaml().unwrap().contains("boncuk"));
    }

    #[test]
    fn nested_path() {
        let doc = Document::parse(
            "veri x { a = [ { b = [10, 20, 30] }, { b = [40, 50, 60] } ] }\n",
        )
        .unwrap();
        assert_eq!(doc.get("x.a[1].b[2]").unwrap(), Value::Int(60));
    }

    #[test]
    fn parse_error() {
        match Document::parse("cins\n") {
            Err(err) => assert_eq!(err.code, ffi::GEN_ERR_PARSE),
            Ok(_) => panic!("expected parse error"),
        }
    }
}
