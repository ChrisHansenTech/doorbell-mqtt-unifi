#pragma once

#include "errors.h"
#include "logger.h"
#include <stdbool.h>

/**
 * @brief Set the current status state.
 * 
 * @param state state string
 */
void status_set_state(const char *state);

/**
 * @brief Set a status message on a given topic.
 * 
 * @param topic topic to publish to
 * @param message status message
 */
void status_set_status_message(const char *topic, const char *message);

/**
 * @brief Set the last error information.
 * 
 * @param code 
 * @param message_override 
 */
void status_set_error(error_code_t code, const char *message_override);

bool status_error_changed(error_code_t code, const char *message);

void status_set_active_profile(const char *profile);



/**
 * @brief Set the availability status.
 * 
 * @param available true if available, false if not
 */
void status_set_availability(bool available);


#define HA_ERR(code, detail) \
    do { \
        const char *_base = error_code_default_message((code)); \
        char _msg[256]; \
        if ((detail) && *(detail)) { \
            snprintf(_msg, sizeof(_msg), "%s: %s", _base, (detail)); \
        } else { \
            snprintf(_msg, sizeof(_msg), "%s", _base); \
        } \
        if (status_error_changed(code, _msg)) { \
            LOG_ERROR("code=%d name=%s msg=%s", (int)(code), error_code_name((code)), _msg); \
         } \
        status_set_error((code), _msg); \
    } while (0)

#define HA_ERRF(code, fmt, ...) \
    do { \
        char _msg[256]; \
        int _n = snprintf(_msg, sizeof(_msg), (fmt), ##__VA_ARGS__); \
        if (_n < 0 || (size_t)_n >= sizeof(_msg)) { \
            snprintf(_msg, sizeof(_msg), "Failed to format error message"); \
        } else if ((size_t)_n >= sizeof(_msg)) { \
            strcpy(_msg + sizeof(_msg) - 4, "..."); \
        } \
        if (status_error_changed(code, _msg)) { \
            LOG_ERROR("code=%d name=%s msg=%s", (int)(code), error_code_name((code)), _msg); \
         } \
        status_set_error((code), _msg); \
    } while (0)
