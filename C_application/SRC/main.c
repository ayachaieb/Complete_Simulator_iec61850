#include "Module_Manager.h"
#include "logger.h"
#include <signal.h>
#include <stdlib.h>
#include "util.h"

// Define a module name for logging
#define MODULE_NAME "Main"
#define DEBUG
volatile int main_application_running = 1;

// Signal handler for graceful shutdown (Ctrl+C)
void handle_sigint(int sig) {
    LOG_INFO(MODULE_NAME, "SIGINT received, initiating graceful shutdown...");
    main_application_running = 0; // Set the flag to stop main loop
}

// Shutdown check callback for the module manager
int check_app_shutdown_status(void) {
    return !main_application_running;
}

int main(void) {
    if (!logger_init(8192, 80)) {
   
        fprintf(stderr, "Failed to initialize logger. Exiting.\n");
        return EXIT_FAILURE;
    }
    
    // Set up signal handler
    signal(SIGINT, handle_sigint);
    
    // Initialize all modules through the module manager
    if (SUCCESS != ModuleManager_init()) {
        error_info_t err = {
            .code = FAIL,
            .description = "Module initialization failed. Exiting."
        };
        LOG_ERROR_CODE(MODULE_NAME, err);
        logger_shutdown();
        return EXIT_FAILURE;
    }
    
    // Log successful initialization
    LOG_INFO(MODULE_NAME, "All modules initialized. Press Ctrl+C to shut down.");
    
    // Run the main application loop through the module manager
    int app_status = ModuleManager_run(check_app_shutdown_status);
    
    // Perform a clean shutdown regardless of how we exited the loop
    LOG_INFO(MODULE_NAME, "Initiating application shutdown...");
    
   if (FAIL == app_status) {
        error_info_t err = {
            .code = app_status,
            .description = "Application terminated with an error."
        };
        LOG_ERROR_CODE(MODULE_NAME, err);
   }
   if(SUCCESS != ModuleManager_shutdown()) {
        error_info_t err = {
            .code = FAIL,
            .description = "Module shutdown failed."
        };
        LOG_ERROR_CODE(MODULE_NAME, err);
         logger_shutdown();
        return EXIT_FAILURE;
    } else {
        LOG_INFO(MODULE_NAME, "Application shutdown complete. Goodbye!");
        logger_flush(); // Ensure all logs are written
        logger_shutdown();
        return EXIT_SUCCESS;
    }
}
