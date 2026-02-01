#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>

static log_level_t g_min_level = LOG_LEVEL_INFO;

static const char *level_to_string(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO: return "INFO ";
        case LOG_LEVEL_WARN: return "WARN ";
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_FATAL: return "FATAL";
        default: return "WEIRD";
    }
}

void log_init(const char *log_level) {
    if (!log_level) {
        log_level = getenv("LOG_LEVEL");
    }

    if (!log_level) {
        g_min_level = LOG_LEVEL_INFO;
        return;
    }

    if (strcasecmp(log_level, "DEBUG") == 0) {
        g_min_level = LOG_LEVEL_DEBUG;
        LOG_WARN("DEBUG logging enabled. Expect verbose output.");
    } else if (strcasecmp(log_level, "INFO") == 0) {
        g_min_level = LOG_LEVEL_INFO;
    } else if (strcasecmp(log_level, "WARN") == 0) {
        g_min_level = LOG_LEVEL_WARN;
    } else if (strcasecmp(log_level, "ERROR") == 0) {
        g_min_level = LOG_LEVEL_ERROR;
    } else {
        g_min_level = LOG_LEVEL_INFO;
        LOG_WARN("Unknown LOG_LEVEL='%s', defaulting to INFO.", log_level);
    }
}

void log_set_level(log_level_t level) {
    g_min_level = level;
}

void log_log_impl(log_level_t level, const char *file, int line, const char *func, const char *fmt, ...) {
    if (level < g_min_level) {
        return;
    }

    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);

    char time_buffer[20];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &tm_info);

    switch (level) {
    case LOG_LEVEL_DEBUG:
    case LOG_LEVEL_FATAL:
        fprintf(stderr, "%s [%s] %s:%d %s: ", time_buffer, level_to_string(level), file, line, func);
        break;
    case LOG_LEVEL_INFO:
        fprintf(stderr, "%s [%s] ", time_buffer, level_to_string(level));
        break;
    case LOG_LEVEL_WARN:
    case LOG_LEVEL_ERROR:
        fprintf(stderr, "%s [%s] %s: ", time_buffer, level_to_string(level), func);
        break;
    default:
        fprintf(stderr, "%s [%s] ", time_buffer, level_to_string(level));
        break;
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fputc('\n', stderr);
    fflush(stderr);

}