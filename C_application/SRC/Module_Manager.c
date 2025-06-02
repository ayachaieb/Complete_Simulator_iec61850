#include "Module_Manager.h"
#include "State_Machine.h"
#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include "util.h"

//  to handle IPC events and route to state machine
static void internal_ipc_event_handler(state_event_e event, const char *requestId) {
    // printf("ModuleManager: IPC event handler received event: %s, requestId: %s\n", 
    //        state_event_to_string(event), requestId ? requestId : "N/A");
    // fflush(stdout);
    StateMachine_push_event(event, requestId);
}

// Initialize all modules in the correct order
int ModuleManager_init(void) {
    printf("ModuleManager: Initializing modules...\n");
    
    // Initialize state machine first
    if ( SUCCESS != StateMachine_Launch()) {
        fprintf(stderr, "ModuleManager: Failed to initialize StateMachineModule.\n");
        return FAIL;
    }
    
    // Initialize IPC after state machine
    int ipc_init_result = ipc_init(internal_ipc_event_handler);
    if (ipc_init_result != SUCCESS) {
        fprintf(stderr, "ModuleManager: Failed to initialize IPC (error code: %d).\n", 
                ipc_init_result);
        StateMachine_shutdown(); // Clean up state machine if IPC fails
        return FAIL;
    }
    
    printf("ModuleManager: All modules initialized successfully.\n");
    return SUCCESS;
}

// Run the main application loop
int ModuleManager_run(shutdown_check_callback_t shutdown_check) {
    if (!shutdown_check) {
        fprintf(stderr, "ModuleManager: Invalid shutdown check callback provided.\n");
        return FAIL;
    }
    
    printf("ModuleManager: Starting main application loop.\n");
    return ipc_run_loop(shutdown_check);
}

// Shutdown all modules in the correct order
int ModuleManager_shutdown(void) {
    printf("ModuleManager: Shutting down all modules...\n");
    
    // Shutdown order is typically reverse of initialization
    if (SUCCESS != ipc_shutdown()) {
        fprintf(stderr, "ModuleManager: Failed to shut down IPC.\n");
        return FAIL;
    }
    if (SUCCESS != StateMachine_shutdown()) {
        fprintf(stderr, "ModuleManager: Failed to shut down StateMachineModule.\n");
        return FAIL;
    }
    
    printf("ModuleManager: All modules shut down successfully.\n");
    return SUCCESS;
}

// Register a custom IPC handler if needed (otherwise internal handler is used)
int ModuleManager_register_ipc_handler(void (*ipc_handler)(int, const char*)) {
    if (!ipc_handler) {
        return FAIL;
    }
    
    ipc_shutdown();
    int ipc_init_result = ipc_init(ipc_handler);
    return ipc_init_result;
}
