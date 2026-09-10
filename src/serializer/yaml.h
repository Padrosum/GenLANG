#ifndef GENLANG_YAML_H
#define GENLANG_YAML_H

#include "runtime/runtime.h"

GenResult gen_value_to_yaml_impl(const GenValue *value, GenStrBuf *buf, int depth);

#endif /* GENLANG_YAML_H */
