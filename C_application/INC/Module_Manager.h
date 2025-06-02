
// module_manager.h
#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

#include <stdbool.h>

// Function pointer type for status check callbacks
typedef int (*shutdown_check_callback_t)(void);

// Initialize the module manager and all required modules
int ModuleManager_init(void);

// Run the main application loop
int ModuleManager_run(shutdown_check_callback_t shutdown_check);

// Shutdown all modules in the correct order
int ModuleManager_shutdown(void);

// Register the IPC event handler
int ModuleManager_register_ipc_handler(void (*ipc_handler)(int, const char*));

#endif
