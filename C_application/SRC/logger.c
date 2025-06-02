#include "logger.h"

// Only compile the full logger implementation in DEBUG mode
#ifdef DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

// Internal buffer structure
typedef struct {
    char* data;
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

// Default log output handler (only needed in debug mode)
static void default_log_handler(const log_entry_t* entry) {
    // Get level string
    const char* level_str = "UNKNOWN";
    switch (entry->level) {
        case LOG_LEVEL_ERROR: level_str = "ERROR"; break;
        case LOG_LEVEL_WARN:  level_str = "WARN"; break;
        case LOG_LEVEL_INFO:  level_str = "INFO"; break;
        case LOG_LEVEL_DEBUG: level_str = "DEBUG"; break;
        case LOG_LEVEL_TRACE: level_str = "TRACE"; break;
        default: break;
    }

    // Format timestamp
    char time_str[32];
    time_t raw_time = entry->timestamp / 1000; // Convert from ms to seconds
    struct tm* time_info = localtime(&raw_time);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);

    // Print to stderr
    fprintf(stderr, "[%s.%03lu] [%s] [%s] %s (%s:%u)\n",
            time_str, entry->timestamp % 1000, level_str, entry->module,
            entry->message, entry->file, entry->line);
}

// Get timestamp (only needed in debug mode)
static uint64_t get_timestamp_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// --- Implementation of Core Logger Functions (Debug Mode) ---

bool logger_init(size_t buffer_size, uint8_t flush_threshold) {
    if (g_log_buffer.initialized) {
        return true; // Already initialized
    }

    // Allocate the buffer
    g_log_buffer.data = (char*)malloc(buffer_size);
    if (!g_log_buffer.data) {
        fprintf(stderr, "[LOGGER INIT ERROR] Failed to allocate log buffer\n");
        return false;
    }

    g_log_buffer.size = buffer_size;
    g_log_buffer.position = 0;
    g_log_buffer.flush_threshold = flush_threshold > 100 ? 100 : flush_threshold;
    g_log_buffer.current_level = LOG_LEVEL_INFO; // Default level
    g_log_buffer.output_handler = default_log_handler;

    // Initialize mutex
    if (pthread_mutex_init(&g_log_buffer.mutex, NULL) != 0) {
        fprintf(stderr, "[LOGGER INIT ERROR] Failed to initialize mutex\n");
        free(g_log_buffer.data);
        g_log_buffer.data = NULL;
        return false;
    }

    g_log_buffer.initialized = true;
    // Optional: Log initialization success using the logger itself
    // LOG_INFO("Logger", "Logger initialized successfully");
    return true;
}

void logger_set_level(log_level_t level) {
    if (g_log_buffer.initialized) {
        pthread_mutex_lock(&g_log_buffer.mutex);
        g_log_buffer.current_level = level;
        pthread_mutex_unlock(&g_log_buffer.mutex);
    }
}

void logger_set_output_handler(log_output_handler_t handler) {
    if (g_log_buffer.initialized) {
        pthread_mutex_lock(&g_log_buffer.mutex);
        g_log_buffer.output_handler = handler ? handler : default_log_handler;
        pthread_mutex_unlock(&g_log_buffer.mutex);
    }
}

// Internal log function (only compiled in debug mode)
void logger_log(log_level_t level, const char* module, const char* file, uint32_t line, const char* message) {
    if (!g_log_buffer.initialized || level > g_log_buffer.current_level) {
        return;
    }

    // Create log entry
    log_entry_t entry = {
        .level = level,
        .module = module,
        .message = message, // Note: Storing pointer, assumes message lifetime is sufficient or copied by handler
        .line = line,
        .file = file,       // Note: Storing pointer
        .timestamp = get_timestamp_ms()
    };

    // Critical section
    pthread_mutex_lock(&g_log_buffer.mutex);

    // Check if there's enough space to store the *entire* entry structure
    // This logger stores the structure directly, not formatted strings.
    size_t entry_size = sizeof(log_entry_t);
    bool needs_flush = false;

    if (g_log_buffer.position + entry_size > g_log_buffer.size) {
        // Not enough space, need to flush before adding the new entry
        needs_flush = true;
    } else {
        // Store the entry
        memcpy(g_log_buffer.data + g_log_buffer.position, &entry, entry_size);
        g_log_buffer.position += entry_size;

        // Check if we need to auto-flush based on threshold or error level
        if (g_log_buffer.flush_threshold > 0) {
            float fullness = (float)g_log_buffer.position / g_log_buffer.size * 100.0f;
            if (fullness >= g_log_buffer.flush_threshold || level == LOG_LEVEL_ERROR) {
                needs_flush = true;
            }
        }
    }

    // Flush if needed (either due to space or threshold/error)
    if (needs_flush && g_log_buffer.position > 0) {
        char* read_pos = g_log_buffer.data;
        char* end_pos = g_log_buffer.data + g_log_buffer.position;
        while (read_pos < end_pos) {
            log_entry_t* current_entry = (log_entry_t*)read_pos;
            if (g_log_buffer.output_handler) {
                g_log_buffer.output_handler(current_entry);
            }
            read_pos += sizeof(log_entry_t); // Move to the next entry
        }
        g_log_buffer.position = 0; // Reset buffer position after flush

        // If we flushed because buffer was full, try adding the new entry again
        if (g_log_buffer.position + entry_size <= g_log_buffer.size) {
             memcpy(g_log_buffer.data + g_log_buffer.position, &entry, entry_size);
             g_log_buffer.position += entry_size;
        } else {
             // Entry is too large even for an empty buffer, or handler failed?
             // Consider logging a meta-error directly to stderr
             fprintf(stderr, "[LOGGER ERROR] Failed to log entry after flush - buffer too small or handler issue?\n");
        }
    }

    pthread_mutex_unlock(&g_log_buffer.mutex);
}

void logger_flush(void) {
    if (!g_log_buffer.initialized || g_log_buffer.position == 0) {
        return;
    }

    pthread_mutex_lock(&g_log_buffer.mutex);

    // Process all entries
    char* read_pos = g_log_buffer.data;
    char* end_pos = g_log_buffer.data + g_log_buffer.position;

    while (read_pos < end_pos) {
        log_entry_t* entry = (log_entry_t*)read_pos;
        if (g_log_buffer.output_handler) {
            g_log_buffer.output_handler(entry);
        }
        read_pos += sizeof(log_entry_t);
    }

    g_log_buffer.position = 0;
    pthread_mutex_unlock(&g_log_buffer.mutex);
}

void logger_shutdown(void) {
    if (!g_log_buffer.initialized) {
        return;
    }

    // Flush any remaining logs
    logger_flush();

    // Clean up
    pthread_mutex_lock(&g_log_buffer.mutex);
    if (g_log_buffer.data) {
        free(g_log_buffer.data);
        g_log_buffer.data = NULL;
    }
    g_log_buffer.size = 0;
    g_log_buffer.initialized = false;
    // Note: Mutex is unlocked *before* destroying
    pthread_mutex_unlock(&g_log_buffer.mutex);

    pthread_mutex_destroy(&g_log_buffer.mutex);
}

#else // NOT DEBUG (Release Mode Stubs)

// In release mode, we need to provide actual function implementations
// that do nothing but allow the code to link successfully.

// We need to include this for the logger_log function
#include <stdarg.h>

// Empty stub implementations for the core functions
bool logger_init(size_t buffer_size, uint8_t flush_threshold) {
    // Do nothing, always return success
    (void)buffer_size;      // Suppress unused parameter warning
    (void)flush_threshold;  // Suppress unused parameter warning
    return true;
}

void logger_set_level(log_level_t level) {
    // Do nothing
    (void)level; // Suppress unused parameter warning
}

void logger_set_output_handler(log_output_handler_t handler) {
    // Do nothing
    (void)handler; // Suppress unused parameter warning
}

void logger_flush(void) {
    // Do nothing
}

void logger_shutdown(void) {
    // Do nothing
}

// We need to provide the logger_log function even though it's not directly
// called by user code (it's called through the macros)
void logger_log(log_level_t level, const char* module, const char* file, uint32_t line, const char* message) {
    // Do nothing in release mode
    (void)level;    // Suppress unused parameter warning
    (void)module;   // Suppress unused parameter warning
    (void)file;     // Suppress unused parameter warning
    (void)line;     // Suppress unused parameter warning
    (void)message;  // Suppress unused parameter warning
}

#endif // DEBUG
