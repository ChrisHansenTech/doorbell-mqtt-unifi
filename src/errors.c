#include "errors.h"

#include <string.h>

typedef struct {
    int code;
    const char* name;
    const char* default_msg;
} error_info_t;

static const error_info_t g_errors[] = {
#define X(code, name, default_msg) { (code), #name, (default_msg) },
#include "errors.def"
#undef X
};

static const int g_errors_count = (int)(sizeof(g_errors) / sizeof(g_errors[0]));

static const error_info_t* find_error(int code) {
    for (int i = 0; i < g_errors_count; i++) {
        if (g_errors[i].code == code) {
            return &g_errors[i];
        }
    }

    return NULL;
}

const char* error_code_name(error_code_t code) {
    const error_info_t* e = find_error((int)code);
    return e ? e->name : "ERR_UNKNOWN";
}

const char* error_code_default_message(error_code_t code) {
    const error_info_t* e = find_error((int)code);
    return e ? e->default_msg : "Unknown error";
}

int error_code_is_known(int code) {
    return find_error(code) != NULL;
}