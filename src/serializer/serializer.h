#ifndef GENLANG_SERIALIZER_H
#define GENLANG_SERIALIZER_H

#include "runtime/runtime.h"

GenResult gen_serialize_document(const GenDocument *document, char **out_text, size_t *out_length);

#endif /* GENLANG_SERIALIZER_H */
