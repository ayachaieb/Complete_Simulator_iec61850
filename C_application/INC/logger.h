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
bool logger_init(size_t buffer_size, uint8_t flush_threshold);

// Set the minimum log level to record
void logger_set_level(log_level_t level);

// Set a custom output handler
void logger_set_output_handler(log_output_handler_t handler);

// Manually flush all buffered log entries
void logger_flush(void);

// Shut down the logger
void logger_shutdown(void);

// --- Conditional Logging Macros ---

#ifdef DEBUG
    // Internal function prototype
    void logger_log(log_level_t level, const char* module, const char* file, uint32_t line, const char* format, ...);

    // Logging macros with variadic arguments
    #define LOG_ERROR(module, ...) logger_log(LOG_LEVEL_ERROR, module, __FILE__, __LINE__, __VA_ARGS__)
    #define LOG_WARN(module, ...)  logger_log(LOG_LEVEL_WARN, module, __FILE__, __LINE__, __VA_ARGS__)
    #define LOG_INFO(module, ...)  logger_log(LOG_LEVEL_INFO, module, __FILE__, __LINE__, __VA_ARGS__)
    #define LOG_DEBUG(module, ...) logger_log(LOG_LEVEL_DEBUG, module, __FILE__, __LINE__, __VA_ARGS__)
    #define LOG_TRACE(module, ...) logger_log(LOG_LEVEL_TRACE, module, __FILE__, __LINE__, __VA_ARGS__)

    // Helper macro for logging error_info_t
    #define LOG_ERROR_CODE(module, error_info) do { \
        char _err_msg[256]; \
        snprintf(_err_msg, sizeof(_err_msg), "Error Code %d: %s", \
                (error_info).code, \
                (error_info).description ? (error_info).description : "(no description)"); \
        logger_log(LOG_LEVEL_ERROR, module, __FILE__, __LINE__, "%s", _err_msg); \
    } while(0)

#else // NOT DEBUG
    // Logging macros - expand to nothing in release mode
    #define LOG_ERROR(module, ...) ((void)0)
    #define LOG_WARN(module, ...)  ((void)0)
    #define LOG_INFO(module, ...)  ((void)0)
    #define LOG_DEBUG(module, ...) ((void)0)
    #define LOG_TRACE(module, ...) ((void)0)
    #define LOG_ERROR_CODE(module, error_info) ((void)0)
#endif // DEBUG

#endif // LOGGER_H