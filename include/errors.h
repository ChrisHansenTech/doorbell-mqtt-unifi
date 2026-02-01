#pragma once

#include <stdint.h>

typedef enum {
#define X(code, name, default_msg) name = (code),
#include "errors.def"
#undef X
} error_code_t;

const char *error_code_name(error_code_t code);
const char *error_code_default_message(error_code_t code);
int error_code_is_known(int code);