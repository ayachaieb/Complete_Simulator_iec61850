#include "logger.h"

// Only compile the full logger implementation in DEBUG mode
#ifdef DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdarg.h>
#include <assert.h>

// Internal buffer structure
typedef struct
{
    char *data;
    size_t size;
    size_t position;
    uint8_t flush_threshold;
    pthread_mutex_t mutex;
    log_level_t current_level;
    log_output_handler_t output_handler;
    bool initialized;
} log_buffer_t;

// Global log buffer (only exists in debug mode)
static log_buffer_t g_log_buffer = {0};

// Default log output handler
static void default_log_handler(const log_entry_t *entry)
{
    const char *level_str = "UNKNOWN";
    switch (entry->level)
    {
    case LOG_LEVEL_ERROR:
        level_str = "ERROR";
        break;
    case LOG_LEVEL_WARN:
        level_str = "WARN";
        break;
    case LOG_LEVEL_INFO:
        level_str = "INFO";
        break;
    case LOG_LEVEL_DEBUG:
        level_str = "DEBUG";
        break;
    case LOG_LEVEL_TRACE:
        level_str = "TRACE";
        break;
    default:
        break;
    }

    char time_str[32];
    time_t raw_time = entry->timestamp / 1000;
    struct tm *time_info = localtime(&raw_time);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);

    fprintf(stderr, "[%s.%03lu] [%s] [%s] %s (%s:%u)\n",
            time_str, entry->timestamp % 1000, level_str, entry->module,
            entry->message, entry->file, entry->line);
}

static uint64_t get_timestamp_ms()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// --- Core Functions Implementation ---

bool logger_init(size_t buffer_size, uint8_t flush_threshold)
{
    if (g_log_buffer.initialized)
    {
        return true;
    }

    g_log_buffer.data = (char *)malloc(buffer_size);
    if (!g_log_buffer.data)
    {
        fprintf(stderr, "LOGGER INIT ERROR: Failed to allocate buffer\n");
        return false;
    }

    g_log_buffer.size = buffer_size;
    g_log_buffer.position = 0;
    g_log_buffer.flush_threshold = (flush_threshold > 100) ? 100 : flush_threshold;
    g_log_buffer.current_level = LOG_LEVEL_INFO;
    g_log_buffer.output_handler = default_log_handler;
    g_log_buffer.initialized = true;

    if (pthread_mutex_init(&g_log_buffer.mutex, NULL) != 0)
    {
        free(g_log_buffer.data);
        g_log_buffer.data = NULL;
        return false;
    }

    return true;
}

void logger_set_level(log_level_t level)
{
    if (g_log_buffer.initialized)
    {
        pthread_mutex_lock(&g_log_buffer.mutex);
        g_log_buffer.current_level = level;
        pthread_mutex_unlock(&g_log_buffer.mutex);
    }
}

void logger_set_output_handler(log_output_handler_t handler)
{
    if (g_log_buffer.initialized)
    {
        pthread_mutex_lock(&g_log_buffer.mutex);
        g_log_buffer.output_handler = handler ? handler : default_log_handler;
        pthread_mutex_unlock(&g_log_buffer.mutex);
    }
}

void logger_flush(void)
{
    if (!g_log_buffer.initialized || g_log_buffer.position == 0)
    {
        return;
    }

    pthread_mutex_lock(&g_log_buffer.mutex);

    char *read_pos = g_log_buffer.data;
    char *end_pos = g_log_buffer.data + g_log_buffer.position;

    while (read_pos < end_pos)
    {
        log_entry_t *entry = (log_entry_t *)read_pos;
        if (g_log_buffer.output_handler)
        {
            g_log_buffer.output_handler(entry);
        }

        if (entry->module)
        {
            free((void *)entry->module); // Cast to void* or char* before freeing const char*
        }
        if (entry->message)
        {
            free((void *)entry->message);
        }
        if (entry->file)
        {
            free((void *)entry->file);
        }

        read_pos += sizeof(log_entry_t);
    }

    g_log_buffer.position = 0;
    pthread_mutex_unlock(&g_log_buffer.mutex);
}

void logger_shutdown(void)
{
    if (!g_log_buffer.initialized)
    {
        return;
    }

    logger_flush();

    pthread_mutex_lock(&g_log_buffer.mutex);
    free(g_log_buffer.data);
    g_log_buffer.data = NULL;
    g_log_buffer.initialized = false;
    pthread_mutex_unlock(&g_log_buffer.mutex);

    pthread_mutex_destroy(&g_log_buffer.mutex);
}

// --- Variadic Logging Function ---

// In logger.c (debug mode)
void logger_log(log_level_t level, const char *module, const char *file,
                uint32_t line, const char *format, ...)
{
    if (!g_log_buffer.initialized || level > g_log_buffer.current_level)
    {
        return;
    }

    // Temporary buffer for message formatting
    char msg_buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(msg_buffer, sizeof(msg_buffer), format, args);
    va_end(args);
    msg_buffer[sizeof(msg_buffer) - 1] = '\0'; // Ensure null-termination

    // Create log entry with string duplication
    log_entry_t entry = {
        .level = level,
        .module = module ? strdup(module) : NULL,
        .message = strdup(msg_buffer),
        .line = line,
        .file = file ? strdup(file) : NULL,
        .timestamp = get_timestamp_ms()};

    pthread_mutex_lock(&g_log_buffer.mutex);

    // Store entry in buffer (ensure proper serialization)
    size_t needed = sizeof(log_entry_t);
    if (g_log_buffer.position + needed > g_log_buffer.size)
    {
        logger_flush();
    }

    if (g_log_buffer.position + needed <= g_log_buffer.size)
    {
        memcpy(g_log_buffer.data + g_log_buffer.position, &entry, needed);
        g_log_buffer.position += needed;
    }
    else
    {
        fprintf(stderr, "LOGGER: Entry too large for buffer\n");
        if (entry.module) free((void*)entry.module);
        if (entry.message) free((void*)entry.message);
        if (entry.file) free((void*)entry.file);
    }

    pthread_mutex_unlock(&g_log_buffer.mutex);
}

#else // Release Mode Stubs

bool logger_init(size_t buffer_size, uint8_t flush_threshold)
{
    (void)buffer_size;
    (void)flush_threshold;
    return true;
}

void logger_set_level(log_level_t level)
{
    (void)level;
}

void logger_set_output_handler(log_output_handler_t handler)
{
    (void)handler;
}

void logger_flush(void) {}

void logger_shutdown(void) {}

void logger_log(log_level_t level, const char *module, const char *file,
                uint32_t line, const char *format, ...)
{
    (void)level;
    (void)module;
    (void)file;
    (void)line;
    (void)format;
}

#endif // DEBUG