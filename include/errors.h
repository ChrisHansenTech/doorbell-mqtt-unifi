#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef enum {
#define X(code, name, default_msg) name = (code),
#include "errors.def"
#undef X
} error_code_t;

typedef struct {
    int error_code;
    char message[128];
} error_return_t;

const char *error_code_name(error_code_t code);
const char *error_code_default_message(error_code_t code);
int error_code_is_known(int code);

static inline error_return_t error_create(int code, const char *message) {
    error_return_t err;
    err.error_code = code;

    if (message) {
        snprintf(err.message, sizeof(err.message), "%s", message);
    } else {
        err.message[0] = '\0';
    }

    return err;
}

static inline error_return_t error_createf(int code, const char *fmt, ...) {
    error_return_t err;
    err.error_code = code;

    if (fmt) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(err.message, sizeof(err.message), fmt, args);
        va_end(args);
    } else {
        err.message[0] = '\0';
    }

    return err;
}

static inline error_return_t error_wrap(error_return_t inner, const char *fmt, ...) {
    error_return_t err;
    err.error_code = inner.error_code;
    err.message[0] = '\0';

    if (fmt) {
        va_list args;
        va_start(args, fmt);
        int written = vsnprintf(err.message, sizeof(err.message), fmt, args);
        va_end(args);

        if (written < 0) {
            strncpy(err.message, inner.message, sizeof(err.message) - 1);
            err.message[sizeof(err.message) - 1] = '\0';
            return err;
        }

        size_t used = strnlen(err.message, sizeof(err.message));
        size_t remaining = sizeof(err.message) - used;

        if (remaining > 1) {
            err.message[used++] = ':';
            remaining--;
        }

        if (remaining > 1) {
            err.message[used++] = ' ';
            remaining--;
        }

        if (remaining > 0) {
            strncpy(err.message + used, inner.message, remaining - 1);
            err.message[sizeof(err.message) - 1] = '\0';
        }
    } else {
        strncpy(err.message, inner.message, sizeof(err.message) - 1);
        err.message[sizeof(err.message) - 1] = '\0';
    }

    return err;
}

static inline error_return_t error_none(void) {
    return error_create(ERROR_NONE, "");
}