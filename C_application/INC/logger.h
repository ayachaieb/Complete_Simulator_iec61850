#ifndef LOGGER_H
#define LOGGER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h> // Needed for snprintf in LOG_ERROR_CODE macro

// Log levels
typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_TRACE
} log_level_t;

// Log entry structure (potentially needed externally)
typedef struct {
    log_level_t level;
    const char* module;
    const char* message;
    uint32_t line;
    const char* file;
    uint64_t timestamp; // Assuming milliseconds since epoch
} log_entry_t;

// Error info structure (potentially needed externally)
typedef struct {
    int code;
    const char* description;
} error_info_t;


// Log output handler function pointer type
typedef void (*log_output_handler_t)(const log_entry_t* entry);

// --- Core Logger Functions ---
// These functions will have stub implementations in release mode

// Initialize the logger
// buffer_size: Size of the internal log entry buffer in bytes.
// flush_threshold: Percentage (0-100) of buffer fullness to trigger auto-flush. 0 disables auto-flush.
bool logger_init(size_t buffer_size, uint8_t flush_threshold);

// Set the minimum log level to record. Messages below this level are ignored.
void logger_set_level(log_level_t level);

// Set a custom output handler. Pass NULL to revert to the default handler (prints to stderr).
void logger_set_output_handler(log_output_handler_t handler);

// Manually flush all buffered log entries to the output handler.
void logger_flush(void);

// Shut down the logger, flush remaining entries, and release resources.
void logger_shutdown(void);

// --- Conditional Logging Macros ---

#ifdef DEBUG
    // Internal function prototype (only needed in debug mode)
    void logger_log(log_level_t level, const char* module, const char* file, uint32_t line, const char* message);

    // Logging macros - expand to function calls in debug mode
    #define LOG_ERROR(module, message) logger_log(LOG_LEVEL_ERROR, module, __FILE__, __LINE__, message)
    #define LOG_WARN(module, message)  logger_log(LOG_LEVEL_WARN,  module, __FILE__, __LINE__, message)
    #define LOG_INFO(module, message)  logger_log(LOG_LEVEL_INFO,  module, __FILE__, __LINE__, message)
    #define LOG_DEBUG(module, message) logger_log(LOG_LEVEL_DEBUG, module, __FILE__, __LINE__, message)
    #define LOG_TRACE(module, message) logger_log(LOG_LEVEL_TRACE, module, __FILE__, __LINE__, message)

    // Helper macro for logging error_info_t
    // Formats the error code and description into a single string before logging.
    #define LOG_ERROR_CODE(module, error_info) do { \
        char _err_msg[256]; \
        snprintf(_err_msg, sizeof(_err_msg), "Error Code %d: %s", (error_info).code, (error_info).description ? (error_info).description : "(no description)"); \
        logger_log(LOG_LEVEL_ERROR, module, __FILE__, __LINE__, _err_msg); \
    } while(0)

#else // NOT DEBUG
    // Logging macros - expand to nothing in release mode
    #define LOG_ERROR(module, message) ((void)0)
    #define LOG_WARN(module, message)  ((void)0)
    #define LOG_INFO(module, message)  ((void)0)
    #define LOG_DEBUG(module, message) ((void)0)
    #define LOG_TRACE(module, message) ((void)0)
    #define LOG_ERROR_CODE(module, error_info) ((void)0)

#endif // DEBUG

#endif // LOGGER_H

