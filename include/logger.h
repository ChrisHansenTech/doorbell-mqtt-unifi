#pragma once

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} log_level_t;

void log_init(const char *log_level);

void log_set_level(log_level_t level);

void log_log_impl(log_level_t level, const char *file, int line, const char *func, const char *fmt, ...) __attribute__((format(printf, 5, 6)));

#define log_log(level, file, line, func, ...) log_log_impl(level, file, line, func, __VA_ARGS__)

#define LOG_DEBUG(...) log_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG_INFO(...) log_log(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG_WARN(...) log_log(LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG_ERROR(...) log_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG_FATAL(...) log_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)

