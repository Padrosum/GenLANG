#ifndef GENLANG_JSON_H
#define GENLANG_JSON_H

#include "runtime/runtime.h"

GenResult gen_value_to_json_impl(const GenValue *value, GenStrBuf *buf);
GenResult gen_document_to_json_impl(const GenDocument *document, GenStrBuf *buf);

#endif /* GENLANG_JSON_H */
