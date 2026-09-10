package genlang

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo LDFLAGS: -L${SRCDIR}/../../build -lgenlang -Wl,-rpath,${SRCDIR}/../../build
#include <stdlib.h>
#include <genlang.h>
*/
import "C"

import (
	"fmt"
	"unsafe"
)

type Ref struct {
	Name string
}

func (r Ref) String() string {
	return "@" + r.Name
}

type Error struct {
	Code    int
	Message string
	Line    int
	Column  int
	Path    string
}

func (e *Error) Error() string {
	if e.Line > 0 {
		loc := e.Path
		if loc == "" {
			loc = "<input>"
		}
		return fmt.Sprintf("%s:%d:%d: %s", loc, e.Line, e.Column, e.Message)
	}
	return e.Message
}

func Version() string {
	return C.GoString(C.gen_version())
}

func errorFromCtx(ctx *C.GenContext, code C.GenResult) error {
	err := C.gen_context_last_error(ctx)
	path := C.GoString(C.gen_error_path(err))
	return &Error{
		Code:    int(code),
		Message: C.GoString(C.gen_error_message(err)),
		Line:    int(C.gen_error_line(err)),
		Column:  int(C.gen_error_column(err)),
		Path:    path,
	}
}

func valueFromC(ptr *C.GenValue) any {
	if ptr == nil {
		return nil
	}
	switch C.gen_value_type(ptr) {
	case C.GEN_VALUE_NULL:
		return nil
	case C.GEN_VALUE_BOOL:
		return C.gen_value_bool(ptr) != 0
	case C.GEN_VALUE_INT:
		return int64(C.gen_value_int(ptr))
	case C.GEN_VALUE_FLOAT:
		return float64(C.gen_value_float(ptr))
	case C.GEN_VALUE_STRING:
		return C.GoString(C.gen_value_string(ptr))
	case C.GEN_VALUE_REFERENCE:
		return Ref{Name: C.GoString(C.gen_value_reference(ptr))}
	case C.GEN_VALUE_LIST:
		n := int(C.gen_value_list_count(ptr))
		out := make([]any, 0, n)
		for i := 0; i < n; i++ {
			var item *C.GenValue
			C.gen_value_list_get(ptr, C.size_t(i), &item)
			out = append(out, valueFromC(item))
		}
		return out
	case C.GEN_VALUE_OBJECT:
		n := int(C.gen_value_object_count(ptr))
		out := make(map[string]any, n)
		for i := 0; i < n; i++ {
			var key *C.char
			var val *C.GenValue
			C.gen_value_object_key(ptr, C.size_t(i), &key)
			C.gen_value_object_get_index(ptr, C.size_t(i), &val)
			out[C.GoString(key)] = valueFromC(val)
		}
		return out
	default:
		return nil
	}
}

func namesFromQuery(rc C.GenResult, result *C.GenQueryResult) ([]string, error) {
	if rc != C.GEN_OK {
		return nil, &Error{Code: int(rc), Message: "query failed"}
	}
	n := int(C.gen_query_result_count(result))
	names := make([]string, 0, n)
	for i := 0; i < n; i++ {
		names = append(names, C.GoString(C.gen_query_result_name(result, C.size_t(i))))
	}
	C.gen_query_result_free(result)
	return names, nil
}

type Document struct {
	ctx *C.GenContext
	doc *C.GenDocument
}

func Parse(source string) (*Document, error) {
	ctx := C.gen_context_create()
	if ctx == nil {
		return nil, &Error{Code: int(C.GEN_ERR_OUT_OF_MEMORY), Message: "out of memory"}
	}
	csrc := C.CString(source)
	defer C.free(unsafe.Pointer(csrc))
	var doc *C.GenDocument
	rc := C.gen_document_parse(ctx, csrc, &doc)
	if rc != C.GEN_OK {
		err := errorFromCtx(ctx, rc)
		C.gen_context_free(ctx)
		return nil, err
	}
	return &Document{ctx: ctx, doc: doc}, nil
}

func Load(path string) (*Document, error) {
	ctx := C.gen_context_create()
	if ctx == nil {
		return nil, &Error{Code: int(C.GEN_ERR_OUT_OF_MEMORY), Message: "out of memory"}
	}
	cpath := C.CString(path)
	defer C.free(unsafe.Pointer(cpath))
	var doc *C.GenDocument
	rc := C.gen_document_load_file(ctx, cpath, &doc)
	if rc != C.GEN_OK {
		err := errorFromCtx(ctx, rc)
		C.gen_context_free(ctx)
		return nil, err
	}
	return &Document{ctx: ctx, doc: doc}, nil
}

func (d *Document) Close() {
	if d.doc != nil {
		C.gen_document_free(d.doc)
		d.doc = nil
	}
	if d.ctx != nil {
		C.gen_context_free(d.ctx)
		d.ctx = nil
	}
}

func listNames(
	n C.size_t,
	fn func(C.size_t, **C.char) C.GenResult,
) []string {
	count := int(n)
	out := make([]string, 0, count)
	for i := 0; i < count; i++ {
		var name *C.char
		fn(C.size_t(i), &name)
		out = append(out, C.GoString(name))
	}
	return out
}

func (d *Document) Types() []string {
	return listNames(C.gen_type_count(d.doc), func(i C.size_t, name **C.char) C.GenResult {
		return C.gen_type_name(d.doc, i, name)
	})
}

func (d *Document) Entities() []string {
	return listNames(C.gen_entity_count(d.doc), func(i C.size_t, name **C.char) C.GenResult {
		return C.gen_entity_name(d.doc, i, name)
	})
}

func (d *Document) TypeParent(name string) (string, bool, error) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	var parent *C.char
	rc := C.gen_type_parent(d.doc, cname, &parent)
	if rc != C.GEN_OK {
		return "", false, &Error{Code: int(rc), Message: "unknown type '" + name + "'"}
	}
	if parent == nil {
		return "", false, nil
	}
	return C.GoString(parent), true, nil
}

func (d *Document) EntityType(name string) (string, bool, error) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	var ty *C.char
	rc := C.gen_entity_type_name(d.doc, cname, &ty)
	if rc != C.GEN_OK {
		return "", false, &Error{Code: int(rc), Message: "unknown entity '" + name + "'"}
	}
	if ty == nil {
		return "", false, nil
	}
	return C.GoString(ty), true, nil
}

func (d *Document) Entity(name string) (any, error) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	var value *C.GenValue
	rc := C.gen_entity_value(d.doc, cname, &value)
	if rc != C.GEN_OK {
		return nil, &Error{Code: int(rc), Message: "unknown entity '" + name + "'"}
	}
	return valueFromC(value), nil
}

func (d *Document) Get(path string) (any, error) {
	cpath := C.CString(path)
	defer C.free(unsafe.Pointer(cpath))
	var value *C.GenValue
	rc := C.gen_get(d.doc, cpath, &value)
	if rc != C.GEN_OK {
		return nil, &Error{Code: int(rc), Message: "cannot evaluate path '" + path + "'"}
	}
	defer C.gen_value_free(value)
	return valueFromC(value), nil
}

func (d *Document) Ancestors(name string) ([]string, error) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	var result *C.GenQueryResult
	rc := C.gen_ancestors_of(d.doc, cname, &result)
	return namesFromQuery(rc, result)
}

func (d *Document) Members(setName string) ([]string, error) {
	cname := C.CString(setName)
	defer C.free(unsafe.Pointer(cname))
	var result *C.GenQueryResult
	rc := C.gen_set_members(d.doc, cname, &result)
	return namesFromQuery(rc, result)
}

func (d *Document) IsMember(setName, entity string) (bool, error) {
	cset := C.CString(setName)
	defer C.free(unsafe.Pointer(cset))
	cent := C.CString(entity)
	defer C.free(unsafe.Pointer(cent))
	var flag C.int
	rc := C.gen_is_member(d.doc, cset, cent, &flag)
	if rc != C.GEN_OK {
		return false, &Error{Code: int(rc), Message: "unknown set '" + setName + "'"}
	}
	return flag != 0, nil
}

func (d *Document) Serialize() (string, error) {
	var text *C.char
	rc := C.gen_document_serialize(d.doc, &text, nil)
	if rc != C.GEN_OK {
		return "", &Error{Code: int(rc), Message: "serialize failed"}
	}
	defer C.gen_string_free(text)
	return C.GoString(text), nil
}

func (d *Document) ToJSON() (string, error) {
	var text *C.char
	rc := C.gen_document_to_json(d.doc, &text, nil)
	if rc != C.GEN_OK {
		return "", &Error{Code: int(rc), Message: "json conversion failed"}
	}
	defer C.gen_string_free(text)
	return C.GoString(text), nil
}

func (d *Document) ToYAML() (string, error) {
	var text *C.char
	rc := C.gen_document_to_yaml(d.doc, &text, nil)
	if rc != C.GEN_OK {
		return "", &Error{Code: int(rc), Message: "yaml conversion failed"}
	}
	defer C.gen_string_free(text)
	return C.GoString(text), nil
}
