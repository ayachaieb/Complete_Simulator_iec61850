#include "State_Machine.h" 
#include "ipc.h"    
#include "State_Machine.h"
#include "util.h"    
#include <stdio.h>
#include <signal.h> 
#include <stdlib.h> 
#include <cjson/cJSON.h>

volatile int main_application_running = 1;

// Signal handler for graceful shutdown (Ctrl+C)
void handle_sigint(int sig) {
    printf("\nMain: SIGINT received, initiating graceful shutdown...\n");
    main_application_running = 0; // Set the flag to stop main loop
}


int check_app_shutdown_status(void) {
    return !main_application_running;
}

// Callback function 
void ipc_event_handler(state_event_e event, const char *requestId) {
    printf("Main: IPC event handler received event: %s, requestId: %s\n", state_event_to_string(event), requestId ? requestId : "N/A");
    StateMachine_push_event(event,requestId);
}


int main(void) {
  
    signal(SIGINT, handle_sigint);

 
    if (StateMachine_Launch() != 0) {
        fprintf(stderr, "Main: Failed to initialize StateMachineModule. Exiting.\n");
        return EXIT_FAILURE;
    }



    if (ipc_init(ipc_event_handler) != 0) {
        fprintf(stderr, "Main: Failed to initialize ipc. Shutting down StateMachineModule.\n");
        StateMachine_shutdown(); // Clean up state machine if IPC fails
        return EXIT_FAILURE;
    }
 

    printf("Main: Starting IPC communication loop. Press Ctrl+C to shut down.\n");
     fflush(stdout); 
    int ipc_loop_status = ipc_run_loop(check_app_shutdown_status);

    if (ipc_loop_status == -1) {
        fprintf(stderr, "Main: IPC communication loop terminated with an error.\n");
    } else {
        printf("Main: IPC communication loop exited gracefully.\n");
    }

    // 5. Initiate graceful shutdown of all modules
    printf("Main: Initiating full application shutdown...\n");
    ipc_shutdown();    //  shut down the IPC listener
    StateMachine_shutdown(); // Then, shut down the state machine

    printf("Main: Application shutdown complete. Goodbye!\n");
    return EXIT_SUCCESS;
}