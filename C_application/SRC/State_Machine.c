#include "State_Machine.h"
#include <stdio.h>
#include "enum_to_string.h"
#include "pthread.h"
#include "Ring_Buffer.h"
static state_machine_t sm_data_internal;
static EventQueue event_queue_internal;
static pthread_t sm_thread_internal;

// Function prototypes for state handlers
void state_idle_init(void *data);
void state_idle_enter(void *data, state_e from, state_event_e event);

void state_init_init(void *data);
void state_init_enter(void *data, state_e from, state_event_e event);

void state_running_init(void *data);
void state_running_enter(void *data, state_e from, state_event_e event);

void state_stop_init(void *data);
void state_stop_enter(void *data, state_e from, state_event_e event);

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
        sm->handlers[sm->current_state].enter( NULL,STATE_IDLE, STATE_EVENT_NONE);
    }
}


void state_machine_run(state_machine_t *sm, state_event_e event)
{
    state_e current = sm->current_state;
    state_e next = current;

    // Transition logic
    switch (current) {
        case STATE_IDLE:
            if (event == STATE_EVENT_start_simulation) {
                next = STATE_INIT;
            }
            else if (event == STATE_EVENT_shutdown) {
                next = STATE_STOP;
            }
            break;
        case STATE_INIT:
            if (event == STATE_EVENT_init_success) {
                next = STATE_RUNNING;
            } else if (event == STATE_EVENT_init_failed) {
                next = STATE_IDLE;
            } else if (event == STATE_EVENT_stop_simulation) {
                next = STATE_STOP;
            }                                                                                                             
            break;
        case STATE_RUNNING:
            if (event == STATE_EVENT_stop_simulation) {
                next = STATE_IDLE;
            }
            else if (event == STATE_EVENT_pause_simulation) {
                next = STATE_INIT;
            } else if (event == STATE_EVENT_stop_simulation) {
                next = STATE_STOP; }
            break;
        case STATE_STOP:
        printf("State STOP\n");
            break;
        default:
            break;
    }

    if (next != current || event != STATE_EVENT_NONE) {
        state_enter(sm, next, current, event);
    }
}

void state_enter(state_machine_t *sm, state_e to, state_e from, state_event_e event)
{
    if (from != to) {
        printf("State changed from %s to %s due to %s\n",
               state_to_string(from), state_to_string(to), state_event_to_string(event));
    }
    if (sm->handlers[to].enter) {
        sm->handlers[to].enter(NULL, from, event);
    }
    sm->current_state = to;
}

void state_idle_init(void *data)
{
    
    // Initialize idle state data here
}

void state_idle_enter( void *data,state_e from, state_event_e event)
{
   // TRACE("Entered IDLE state from %s due to %s", state_to_string(from), state_event_to_string(event));
    printf("Entered IDLE state from %s due to %s", state_to_string(from), state_event_to_string(event));
    // IDLE state entry logic here
}

void state_init_init(void *data)
{
    // initialisation
}

void state_init_enter(void *data ,state_e from, state_event_e event)
{
    printf("Entered INIT state from %s due to %s", state_to_string(from), state_event_to_string(event));
    // INIT state entry logic  
}

void state_running_init(void *data)
{
    // Initialize running state data here
}

void state_running_enter(void *data, state_e from, state_event_e event)
{
    printf("Entered RUNNING state from %s due to %s", state_to_string(from), state_event_to_string(event));
    // Add your RUNNING state entry logic here
}

void state_stop_init(void *data)
{
    //implementation
}

void state_stop_enter(void *data , state_e from, state_event_e event)
{
    printf("Entered STOP state from %s due to %s", state_to_string(from), state_event_to_string(event));
    // implementation
}

// State machine thread function (made static as it's internal to the module)
static void *state_machine_thread_internal(void *arg) {
    state_machine_t *sm = (state_machine_t *)arg;
    if (!sm) {
        fprintf(stderr, "State machine pointer is NULL in internal thread\n");
        return NULL;
    }
    state_machine_init(sm); 
    while (1) {
        state_event_e event = event_queue_pop(&event_queue_internal); 
        if (event == STATE_EVENT_shutdown) {
            break;
        }
        state_machine_run(sm, event); 
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

void StateMachine_push_event(state_event_e event) {
    event_queue_push(event, &event_queue_internal); 
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