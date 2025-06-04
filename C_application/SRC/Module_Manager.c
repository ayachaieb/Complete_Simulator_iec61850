#include "Module_Manager.h"
#include "State_Machine.h"
#include "ipc.h"
#include <stdlib.h>
#include "util.h"
#include "logger.h"


int ModuleManager_init(void) {
    LOG_INFO("ModuleManager", "Initializing modules...");
    
    if (SUCCESS != StateMachine_Launch()) {
        LOG_ERROR("ModuleManager", "Failed to initialize StateMachineModule");
        return FAIL;
    }
    
    int ipc_init_result = ipc_init();
    if (ipc_init_result != SUCCESS) {
        LOG_ERROR("ModuleManager", "Failed to initialize IPC (error code: %d)", ipc_init_result);
        StateMachine_shutdown();
        return FAIL;
    }
    
    LOG_INFO("ModuleManager", "All modules initialized successfully");
    return SUCCESS;
}

// Run the main application loop
int ModuleManager_run(shutdown_check_callback_t shutdown_check) {
    if (!shutdown_check) {
        LOG_ERROR("ModuleManager", "Invalid shutdown check callback provided");
        return FAIL;
    }
    
    LOG_INFO("ModuleManager", "Starting main application loop");
    return ipc_run_loop(shutdown_check);
}

// Shutdown all modules in the correct order
int ModuleManager_shutdown(void) {
    LOG_INFO("ModuleManager", "Shutting down all modules...");
    
    // Shutdown order is typically reverse of initialization
    if (SUCCESS != ipc_shutdown()) {
        LOG_ERROR("ModuleManager", "Failed to shut down IPC");
        return FAIL;
    }
    if (SUCCESS != StateMachine_shutdown()) {
        LOG_ERROR("ModuleManager", "Failed to shut down StateMachineModule");
        return FAIL;
    }
    
    LOG_INFO("ModuleManager", "All modules shut down successfully");
    return SUCCESS;
}

// Register a custom IPC handler if needed
int ModuleManager_register_ipc_handler(void (*ipc_handler)(int, const char*)) {
    if (!ipc_handler) {
        LOG_ERROR("ModuleManager", "Invalid IPC handler provided");
        return FAIL;
    }
    
    ipc_shutdown();
    int ipc_init_result = ipc_init(ipc_handler);
    
    if (ipc_init_result != SUCCESS) {
        LOG_ERROR("ModuleManager", "Failed to register custom IPC handler");
    } else {
        LOG_INFO("ModuleManager", "Custom IPC handler registered successfully");
    }
    
    return ipc_init_result;
}