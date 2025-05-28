#include "State_Machine.h" 
#include "ipc.h"    
#include "State_Machine.h"     
#include <stdio.h>
#include <signal.h> 
#include <stdlib.h> 
#include <cjson/cJSON.h>

// Global flag for main loop shutdown, controlled by signal handler
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
    printf("Main: IPC event handler received event: %d, requestId: %s\n", event, requestId ? requestId : "N/A");

   
 
    StateMachine_push_event(event);

    cJSON *json_response = cJSON_CreateObject();
    if (!json_response) {
        fprintf(stderr, "Main: Failed to create JSON response object for event handler.\n");
        return;
    }

    const char *status_msg = "Event received, processing...";
    switch (event) {
        case STATE_EVENT_start_simulation:
            status_msg = "Simulation started successfully";
            break;
        case STATE_EVENT_stop_simulation:
            status_msg = "Simulation stopped successfully";
            break;
        case STATE_EVENT_pause_simulation:
            status_msg = "Simulation paused successfully";
            break;
        // Add other event statuses here
        default:
            status_msg = "Unknown event received";
            break;
    }

    cJSON_AddStringToObject(json_response, "status", status_msg);
    if (requestId) {
        cJSON_AddStringToObject(json_response, "requestId", requestId);
    }

    char *response_str = cJSON_PrintUnformatted(json_response);
    if (response_str) {
        if(ipc_send_response(response_str)==-1) {
            fprintf(stderr, "Main: Failed to send response: %s\n", response_str);
        } else {
            printf("Main: Response sent successfully: %s\n", response_str);
        }
        free(response_str); // Free the string allocated by cJSON_PrintUnformatted
    } else {
        fprintf(stderr, "Main: Failed to serialize JSON response in event handler.\n");
    }

    cJSON_Delete(json_response); // Free the cJSON object
}


int main(void) {
  
    signal(SIGINT, handle_sigint);
    printf("Main: Registered SIGINT handler for graceful shutdown.\n");
 
    if (StateMachine_Launch() != 0) {
        fprintf(stderr, "Main: Failed to initialize StateMachineModule. Exiting.\n");
        return EXIT_FAILURE;
    }
    printf("Main: StateMachineModule launched.\n");


    if (ipc_init(ipc_event_handler) != 0) {
        fprintf(stderr, "Main: Failed to initialize ipc. Shutting down StateMachineModule.\n");
        StateMachine_shutdown(); // Clean up state machine if IPC fails
        return EXIT_FAILURE;
    }
    printf("Main: ipc initialized.\n");

    // 4. Run the IPC communication loop
    // This function will block until shutdown_check_func returns true or a critical error occurs.
    printf("Main: Starting IPC communication loop. Press Ctrl+C to shut down.\n");
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