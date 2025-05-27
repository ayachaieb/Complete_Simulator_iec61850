#include "State_Machine.h"
#include <stdio.h>
#include "state_idle.h"
#include "state_init.h"
#include "state_running.h"
#include "state_stop.h"
#include "enum_to_string.h"




typedef struct state_machine_data {
    state_e state;
    struct state_idle_data idle;
    struct state_init_data init;
    struct state_running_data running;
    struct state_stop_data stop;
    state_event_e internal_event;
} ;

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