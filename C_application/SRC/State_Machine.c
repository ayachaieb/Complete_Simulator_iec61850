#include "State_Machine.h"
#include <stdio.h>
#include "enum_to_string.h"

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