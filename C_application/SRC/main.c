#include "Module_Manager.h"
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include "util.h"

volatile int main_application_running = 1;

// Signal handler for graceful shutdown (Ctrl+C)
void handle_sigint(int sig) {
    printf("\nMain: SIGINT received, initiating graceful shutdown...\n");
    main_application_running = 0; // Set the flag to stop main loop
}

// Shutdown check callback for the module manager
int check_app_shutdown_status(void) {
    return !main_application_running;
}

int main(void) {
    // Set up signal handler
    signal(SIGINT, handle_sigint);
    
    // Initialize all modules through the module manager
    if (SUCCESS != ModuleManager_init()) {
        fprintf(stderr, "Main: Module initialization failed. Exiting.\n");
        return EXIT_FAILURE;
    }
    // just for debug
    printf("Main: All modules initialized. Press Ctrl+C to shut down.\n");
    fflush(stdout);
    
    // Run the main application loop through the module manager
    int app_status = ModuleManager_run(check_app_shutdown_status);
    
    // Perform a clean shutdown regardless of how we exited the loop
    printf("Main: Initiating application shutdown...\n");
    ModuleManager_shutdown();
    
    if (FAIL == app_status ) {
        fprintf(stderr, "Main: Application terminated with an error.\n");
        return EXIT_FAILURE;
    } else {
        printf("Main: Application shutdown complete. Goodbye!\n");
        return EXIT_SUCCESS;
    }
}
