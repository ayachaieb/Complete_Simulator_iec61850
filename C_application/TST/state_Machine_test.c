#include "State_Machine.h"
#include "util.h"
#include "pthread.h"
#include "ipc.h"
#include <errno.h>
#include "Ring_Buffer.h"
#include <cjson/cJSON.h>
#include <stdlib.h>
// Include the logger header
#include "logger.h" 

static state_machine_t sm_data_internal;
static EventQueue event_queue_internal;
static pthread_t sm_thread_internal;

#define FAIL -1
#define SUCCESS 0

// Function prototypes for state handlers
bool state_idle_init(void *data);
bool state_idle_enter(void *data, state_e from, state_event_e event, const char *requestId);

bool state_init_init(void *data);
bool state_init_enter(void *data, state_e from, state_event_e event, const char *requestId);

bool state_running_init(void *data);
bool state_running_enter(void *data, state_e from, state_event_e event, const char *requestId);

bool state_stop_init(void *data);
bool state_stop_enter(void *data, state_e from, state_event_e event, const char *requestId);
void state_machine_free(state_machine_t *sm)
{
    free(sm->handlers);
    sm->handlers = NULL;
}

int state_machine_init(state_machine_t *sm)
{
    // Allocate handlers array for 4 states
    sm->handlers = calloc(4, sizeof(state_handler_t));
    if (!sm->handlers) {
        // Changed fprintf to logger_log with ERROR level
        LOG_ERROR("State_Machine", "Failed to allocate handlers"); 
        return FAIL;
    }

    sm->current_state = STATE_IDLE;
    // Initialize handlers
    sm->handlers[STATE_IDLE].init = state_idle_init;
    sm->handlers[STATE_IDLE].enter = state_idle_enter;
    sm->handlers[STATE_INIT].init = state_init_init;
    sm->handlers[STATE_INIT].enter = state_init_enter;
    sm->handlers[STATE_RUNNING].init = state_running_init;
    sm->handlers[STATE_RUNNING].enter = state_running_enter;
    sm->handlers[STATE_STOP].init = state_stop_init;
    sm->handlers[STATE_STOP].enter = state_stop_enter;

    // Call the enter function for the initial state
    if (sm->handlers[sm->current_state].enter) {
        sm->handlers[sm->current_state].enter( NULL,STATE_IDLE, STATE_EVENT_NONE,NULL);
    }
    else {
        // Changed fprintf to logger_log with ERROR level
        LOG_ERROR("State_Machine", "Enter handler for initial state %d is NULL", sm->current_state);
        return FAIL;
    }
    return SUCCESS;
}


void state_machine_run(state_machine_t *sm, state_event_e event, const char *requestId)
{
    if (!sm) {
        // Changed fprintf to logger_log with ERROR level
        LOG_ERROR("State_Machine", "State machine pointer is NULL in run function");
        return;
    }

    // Log the event and requestId (using DEBUG for potentially frequent messages)
    LOG_DEBUG("State_Machine", "State machine received event: %s, requestId: %s",
             state_event_to_string(event), requestId ? requestId : "N/A");

    // Handle shutdown event
    if (STATE_EVENT_shutdown ==event) {
        // Changed printf to logger_log with INFO level
        LOG_INFO("State_Machine", "State machine received shutdown event, transitioning to STOP state.");
        sm->current_state = STATE_STOP;
        state_enter(sm, sm->current_state, sm->current_state, event,requestId);
        return;
    }

    state_e current = sm->current_state;
    state_e next = current;

    // Transition logic
    switch (current) {
        case STATE_IDLE:
            if (STATE_EVENT_start_simulation == event ) {
                next = STATE_INIT;
            }
            else if ( STATE_EVENT_shutdown == event ) {
                next = STATE_STOP;
            }
            break;
        case STATE_INIT:
            if (STATE_EVENT_init_success == event ) {
                next = STATE_RUNNING;
            } else if (STATE_EVENT_init_failed == event ) {
                next = STATE_IDLE;
            } else if (STATE_EVENT_stop_simulation == event ) {
                next = STATE_STOP;
            }                                                                                                                                                                                                                                                                                                                                                                                                             
            break;
        case STATE_RUNNING:
            if (STATE_EVENT_stop_simulation == event  ) {
                next = STATE_IDLE;
            }
            else if (STATE_EVENT_pause_simulation == event  ) {
                next = STATE_INIT;
            } else if (STATE_EVENT_stop_simulation == event  ) {
                next = STATE_STOP; 
            }
            break;
        case STATE_STOP:
            // Changed printf to logger_log with INFO level
            LOG_INFO("State_Machine", "State STOP");
            break;
        default:
            break;
    }

    if (next != current || event != STATE_EVENT_NONE) {
        state_enter(sm, next, current, event,requestId);
    }
}

void state_enter(state_machine_t *sm, state_e to, state_e from, state_event_e event, const char *requestId)
{
    if (from != to) {
        // Changed printf to logger_log with INFO level
        LOG_INFO("State_Machine", "State changed from %s to %s due to %s",
                 state_to_string(from), state_to_string(to), state_event_to_string(event));
    
        if (sm->handlers[to].enter) {
            // Changed printf to logger_log with DEBUG level
            LOG_DEBUG("State_Machine", "Calling enter handler for state %s", state_to_string(to));
            sm->handlers[to].enter(NULL, from, event,requestId);
        }
        sm->current_state = to;
    }
}

bool state_idle_init(void *data)
{
    // Initialize idle state data here
    return true; // Assuming successful initialization
}

bool state_idle_enter( void *data,state_e from, state_event_e event, const char *requestId)
{
    // TRACE("Entered IDLE state from %s due to %s", state_to_string(from), state_event_to_string(event));
    // Changed printf to logger_log with INFO level
    LOG_INFO("State_Machine", "Entered IDLE state from %s due to %s", state_to_string(from), state_event_to_string(event));
    // Removed fflush(stdout)
    // IDLE state entry logic here
    return true; // Assuming successful entry
}

bool state_init_init(void *data)
{
    // initialisation  sv initialisation 
    return true; // Assuming successful initialization
}

bool state_init_enter(void *data ,state_e from, state_event_e event, const char *requestId)
{
    // Changed printf to logger_log with INFO level
    LOG_INFO("State_Machine", "Entered INITIATION state from %s due to %s", state_to_string(from), state_event_to_string(event));
    // Removed fflush(stdout)
    
    cJSON *json_response = cJSON_CreateObject();
    if (!json_response) {
        // Changed fprintf to logger_log with ERROR level
        LOG_ERROR("State_Machine", "Failed to create JSON response object for event handler.");
        return false; // Indicate failure
    }
    const char *status_msg = "state init currently executing ...";
    cJSON_AddStringToObject(json_response, "status", status_msg);
    if (requestId) {
        cJSON_AddStringToObject(json_response, "requestId", requestId);
    }

    char *response_str = cJSON_PrintUnformatted(json_response);
    if (response_str) {
        if(ipc_send_response(response_str)== FAIL) {
            // Changed fprintf to logger_log with ERROR level
            LOG_ERROR("State_Machine", "Failed to send response: %s", response_str);
        } else {
            // Changed printf to logger_log with INFO level
            LOG_INFO("State_Machine", "Response sent successfully: %s", response_str);
        }
        free(response_str); // Free the string allocated by cJSON_PrintUnformatted
    } else {
        // Changed fprintf to logger_log with ERROR level
        LOG_ERROR("State_Machine", "Failed to serialize JSON response in event handler.");
    }

    cJSON_Delete(json_response); // Free the cJSON object
    return true; // Assuming successful entry
}

bool state_running_init(void *data)
{
    // Initialize running state data here
    return true; // Assuming successful initialization
}

bool state_running_enter(void *data, state_e from, state_event_e event, const char *requestId)
{
    // Changed printf to logger_log with INFO level
    LOG_INFO("State_Machine", "Entered RUNNING state from %s due to %s", state_to_string(from), state_event_to_string(event));
    // Removed fflush(stdout)
    // Add your RUNNING state entry logic here
    return true; // Assuming successful entry
}

bool state_stop_init(void *data)
{
    //implementation
    return true; // Assuming successful initialization
}

bool state_stop_enter(void *data , state_e from, state_event_e event, const char *requestId)
{
    // Changed printf to logger_log with INFO level
    LOG_INFO("State_Machine", "Entered STOP state from %s due to %s", state_to_string(from), state_event_to_string(event));
    // Removed fflush(stdout)
    
    cJSON *json_response = cJSON_CreateObject();
    if (!json_response) {
        // Changed fprintf to logger_log with ERROR level
        LOG_ERROR("State_Machine", "Failed to create JSON response object for event handler.");
        return false; // Indicate failure
    }
    const char *status_msg = "state STOP currently executing ...";
    cJSON_AddStringToObject(json_response, "status", status_msg);
    if (requestId) {
        cJSON_AddStringToObject(json_response, "requestId", requestId);
    }

    char *response_str = cJSON_PrintUnformatted(json_response);
    if (response_str) {
        if(ipc_send_response(response_str)== FAIL) {
            // Changed fprintf to logger_log with ERROR level
            LOG_ERROR("State_Machine", "Failed to send response: %s", response_str);
        } else {
            // Changed printf to logger_log with INFO level
            LOG_INFO("State_Machine", "Response sent successfully: %s", response_str);
        }
        free(response_str); // Free the string allocated by cJSON_PrintUnformatted
    } else {
        // Changed fprintf to logger_log with ERROR level
        LOG_ERROR("State_Machine", "Failed to serialize JSON response in event handler.");
    }

    cJSON_Delete(json_response); // Free the cJSON object
    return true; // Assuming successful entry
}

// State machine thread function (made static as it's internal to the module)
static void *state_machine_thread_internal(void *arg) {

// ------------------- TEST MAIN FUNCTION -------------------
#include <stdio.h>
int main() {
    printf("\n--- State Machine Test Start ---\n");
    int launch_result = StateMachine_Launch();
    if (launch_result != 0) {
        printf("Failed to launch state machine.\n");
        return 1;
    }
    // Push a sequence of events
    StateMachine_push_event(STATE_EVENT_start_simulation, "REQ1");
    StateMachine_push_event(STATE_EVENT_init_success, "REQ2");
    StateMachine_push_event(STATE_EVENT_stop_simulation, "REQ3");
    StateMachine_push_event(STATE_EVENT_shutdown, "REQ4");
    // Give some time for the thread to process events
    sleep(1);
    StateMachine_shutdown();
    printf("--- State Machine Test End ---\n");
    return 0;
}
// ----------------------------------------------------------
    state_machine_t *sm = (state_machine_t *)arg;
    const char *requestId = NULL; // Initialize requestId to NULL
    if (!sm) {
        // Changed fprintf to logger_log with ERROR level
        LOG_ERROR("State_Machine", "State machine pointer is NULL in internal thread");
        return NULL;
    }
    int init_result =state_machine_init(sm); 
    if (init_result != SUCCESS) {
        // Changed fprintf to logger_log with ERROR level
        LOG_ERROR("State_Machine", "State machine initialization failed in internal thread");
        return NULL;
    }
    while (1) {
        state_event_e event ;
        if (SUCCESS == event_queue_pop(&event_queue_internal,&event, &requestId)) {
            // If event is successfully popped, print it
            // Changed printf to logger_log with DEBUG level
            LOG_DEBUG("State_Machine", "State machine thread popped event: %s, requestId: %s",
                      state_event_to_string(event), requestId ? requestId : "N/A");
        } else {
            // Changed fprintf to logger_log with ERROR level
            LOG_ERROR("State_Machine", "Failed to pop event from queue in internal thread");
            return NULL; 
        }
        if (STATE_EVENT_shutdown == event  ) {
            break;
        }
        state_machine_run(sm, event, requestId); // Run the state machine with the popped event
    }
    state_machine_free(sm);
    return NULL;
}

int StateMachine_Launch(void) {
 
    sm_data_internal.current_state = STATE_IDLE; 
    sm_data_internal.handlers = NULL; 
    // to be modified
    if (SUCCESS != (&event_queue_internal)) {
     
        LOG_ERROR("State_Machine", "Failed to initialize event queue in module");
        return FAIL; 
    }


    if (pthread_create(&sm_thread_internal, NULL, state_machine_thread_internal, &sm_data_internal) != 0) {

        LOG_ERROR("State_Machine", "Failed to create state machine thread in module: %s", strerror(errno)); 
        return FAIL;
    }
  
    LOG_INFO("State_Machine", "Created state machine thread in module");
    return EXIT_SUCCESS; 
}

int StateMachine_push_event(state_event_e event , const char *requestId) {

    int result_event_queue_push = event_queue_push(event, requestId,&event_queue_internal); 
    return result_event_queue_push;
}

int verif_shutdown(void) {
    return event_queue_internal.shutdown;
}

int StateMachine_shutdown(void) {

    LOG_INFO("State_Machine", "Shutting down StateMachine module...");
    event_queue_internal.shutdown = EXIT_FAILURE; 

    int c1 = pthread_cond_signal(&event_queue_internal.cond); // Signal to wake up the thread
    int c2 = pthread_join(sm_thread_internal, NULL); // Wait for the thread to finish
    int c3 = pthread_mutex_destroy(&event_queue_internal.mutex); // Cleanup mutex
    int c4 = pthread_cond_destroy(&event_queue_internal.cond);   // Cleanup cond var
    LOG_INFO("State_Machine", "StateMachine module shutdown complete.");
    if (c1 != 0 || c2 != 0 || c3 != 0 || c4 != 0) {

        LOG_ERROR("State_Machine", "Error during StateMachine shutdown: cond_signal=%d, join=%d, mutex_destroy=%d, cond_destroy=%d",
                     c1, c2, c3,c4);
        return EXIT_FAILURE; // Indicate failure
    }

    return EXIT_SUCCESS; 
}

