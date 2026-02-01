#pragma once

#include "logger.h"

#define LOG_ERR(code, fmt, ...) \
    LOG_ERROR("code=%d name=%s " fmt, (int)(code), error_code_name((code)), ##__VA_ARGS__)

#define LOG_ERR_DEFAULT(code) \
    LOG_ERROR("code=%d name=%s msg=%s", (int)(code), error_code_name((code)), error_code_default_msg((code)))

/* Optional warning variant if you want coded warnings too */
#define LOG_WARN_CODE(code, fmt, ...) \
    LOG_WARN("code=%d name=%s " fmt, (int)(code), error_code_name((code)), ##__VA_ARGS__)