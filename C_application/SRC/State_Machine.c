#include "State_Machine.h"
#include <stdio.h>
#include "util.h"
#include "pthread.h"
#include "ipc.h"
#include "Ring_Buffer.h"
#include <cjson/cJSON.h>
static state_machine_t sm_data_internal;
static EventQueue event_queue_internal;
static pthread_t sm_thread_internal;
#define FAIL -1
#define SUCCESS 0
// Function prototypes for state handlers
void state_idle_init(void *data);
void state_idle_enter(void *data, state_e from, state_event_e event, const char *requestId);

void state_init_init(void *data);
void state_init_enter(void *data, state_e from, state_event_e event, const char *requestId);

void state_running_init(void *data);
void state_running_enter(void *data, state_e from, state_event_e event, const char *requestId);

void state_stop_init(void *data);
void state_stop_enter(void *data, state_e from, state_event_e event, const char *requestId);

void state_machine_free(state_machine_t *sm)
{
    free(sm->handlers);
    sm->handlers = NULL;
}

void state_machine_init(state_machine_t *sm)
{
    // Allocate handlers array for 4 states
    sm->handlers = calloc(4, sizeof(state_handler_t));
    if (!sm->handlers) {
        fprintf(stderr, "Failed to allocate handlers\n");
        exit(1);
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

    // Initialize each state
    for (int i = 0; i < 4; i++) {
        if (sm->handlers[i].init) {
            sm->handlers[i].init(NULL);
        }
    }

    // Call the enter function for the initial state
    if (sm->handlers[sm->current_state].enter) {
        sm->handlers[sm->current_state].enter( NULL,STATE_IDLE, STATE_EVENT_NONE,NULL);
    }
}


void state_machine_run(state_machine_t *sm, state_event_e event, const char *requestId)
{
    if (!sm) {
        fprintf(stderr, "State machine pointer is NULL in run function\n");
        return;
    }

    // Log the event and requestId
   /* printf("State machine received event: %s, requestId: %s\n",
           state_event_to_string(event), requestId ? requestId : "N/A");*/

    // Handle shutdown event
    if (STATE_EVENT_shutdown ==event) {
        printf("State machine received shutdown event, transitioning to STOP state.\n");
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
                next = STATE_STOP; }
            break;
        case STATE_STOP:
        printf("State STOP\n");
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
        // printf("StateMachine-state_enter::State changed from %s to %s due to %s\n",
        //        state_to_string(from), state_to_string(to), state_event_to_string(event));
    
    if (sm->handlers[to].enter) {
       // printf("StateMachine-state_enter::Calling enter handler for state %s\n", state_to_string(to));
        sm->handlers[to].enter(NULL, from, event,requestId);
    }
    sm->current_state = to;
}
}

void state_idle_init(void *data)
{
    
    // Initialize idle state data here
}

void state_idle_enter( void *data,state_e from, state_event_e event, const char *requestId)
{
   // TRACE("Entered IDLE state from %s due to %s", state_to_string(from), state_event_to_string(event));
    printf("Entered IDLE state from %s due to %s", state_to_string(from), state_event_to_string(event));
     fflush(stdout); // Ensure this message is written out immediately
    // IDLE state entry logic here
}

void state_init_init(void *data)
{
    // initialisation  sv initialisation 
}

void state_init_enter(void *data ,state_e from, state_event_e event, const char *requestId)
{
    printf("Entered iniiiit state from %s due to %s", state_to_string(from), state_event_to_string(event));
     fflush(stdout); // Ensure this message is written out immediately
      
      cJSON *json_response = cJSON_CreateObject();
    if (!json_response) {
        fprintf(stderr, "Main: Failed to create JSON response object for event handler.\n");
        return;
    }
  const char *status_msg = "state init currently executing ...";
    cJSON_AddStringToObject(json_response, "status", status_msg);
    if (requestId) {
        cJSON_AddStringToObject(json_response, "requestId", requestId);
    }

    char *response_str = cJSON_PrintUnformatted(json_response);
    if (response_str) {
        if(ipc_send_response(response_str)== FAIL) {
            fprintf(stderr, "State_Machine: Failed to send response: %s\n", response_str);
        } else {
            printf("State_Machine: Response sent successfully: %s\n", response_str);
        }
        free(response_str); // Free the string allocated by cJSON_PrintUnformatted
    } else {
        fprintf(stderr, "State_Machine: Failed to serialize JSON response in event handler.\n");
    }

    cJSON_Delete(json_response); // Free the cJSON object

}

void state_running_init(void *data)
{
    // Initialize running state data here
}

void state_running_enter(void *data, state_e from, state_event_e event, const char *requestId)
{
    printf("Entered RUNNING state from %s due to %s", state_to_string(from), state_event_to_string(event));
     fflush(stdout); // Ensure this message is written out immediately
    // Add your RUNNING state entry logic here
}

void state_stop_init(void *data)
{
    //implementation
}

void state_stop_enter(void *data , state_e from, state_event_e event, const char *requestId)
{
      printf("Entered stopp state from %s due to %s", state_to_string(from), state_event_to_string(event));
     fflush(stdout); // Ensure this message is written out immediately
      
      cJSON *json_response = cJSON_CreateObject();
    if (!json_response) {
        fprintf(stderr, "Main: Failed to create JSON response object for event handler.\n");
        return;
    }
  const char *status_msg = "state STOP currently executing ...";
    cJSON_AddStringToObject(json_response, "status", status_msg);
    if (requestId) {
        cJSON_AddStringToObject(json_response, "requestId", requestId);
    }

    char *response_str = cJSON_PrintUnformatted(json_response);
    if (response_str) {
        if(ipc_send_response(response_str)== FAIL) {
            fprintf(stderr, "State_Machine: Failed to send response: %s\n", response_str);
        } else {
            printf("State_Machine: Response sent successfully: %s\n", response_str);
        }
        free(response_str); // Free the string allocated by cJSON_PrintUnformatted
    } else {
        fprintf(stderr, "State_Machine: Failed to serialize JSON response in event handler.\n");
    }

    cJSON_Delete(json_response); // Free the cJSON object
}

// State machine thread function (made static as it's internal to the module)
static void *state_machine_thread_internal(void *arg) {
    state_machine_t *sm = (state_machine_t *)arg;
    const char *requestId = NULL; // Initialize requestId to NULL
    if (!sm) {
        fprintf(stderr, "State machine pointer is NULL in internal thread\n");
        return NULL;
    }
    state_machine_init(sm); 
    while (1) {
        state_event_e event = event_queue_pop(&event_queue_internal, &requestId); // Pop event from the queue
        printf("State machine thread popped event: %s, requestId: %s\n",
               state_event_to_string(event), requestId ? requestId : "N/A");
        if (STATE_EVENT_shutdown == event  ) {
            break;
        }
        state_machine_run(sm, event, requestId); // Run the state machine with the popped event
    }
    state_machine_free(sm);
    return NULL;
}

int StateMachine_Launch(void) {
    // Initialize internal structures
    sm_data_internal.current_state = STATE_IDLE; 
    sm_data_internal.handlers = NULL; 
    event_queue_init(&event_queue_internal);

    // Create state machine thread
    if (pthread_create(&sm_thread_internal, NULL, state_machine_thread_internal, &sm_data_internal) != 0) {
        perror("Failed to create state machine thread in module");
        return -1; // Indicate failure
    }
    printf("Created state machine thread in module\n");
    return 0; // Indicate success
}

void StateMachine_push_event(state_event_e event , const char *requestId) {
    event_queue_push(event, requestId,&event_queue_internal); 
}
int verif_shutdown(void) {
    return event_queue_internal.shutdown;
}

void StateMachine_shutdown(void) {
    printf("Shutting down StateMachine module...\n");
    event_queue_internal.shutdown = 1;
    pthread_cond_signal(&event_queue_internal.cond); // Signal to wake up the thread
    pthread_join(sm_thread_internal, NULL); // Wait for the thread to finish
    pthread_mutex_destroy(&event_queue_internal.mutex); // Cleanup mutex
    pthread_cond_destroy(&event_queue_internal.cond);   // Cleanup cond var
    printf("StateMachine module shutdown complete.\n");
}