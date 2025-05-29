#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H


typedef struct state_machine state_machine_t;
// typedef struct state_machine_data state_machine_data_t;
typedef struct state_handler state_handler_t;
typedef enum {
    STATE_IDLE,
    STATE_INIT,
    STATE_RUNNING,
    STATE_STOP
} state_e;
typedef enum {
    STATE_EVENT_start_simulation,
    STATE_EVENT_init_success,
    STATE_EVENT_init_failed,
    STATE_EVENT_stop_simulation,
    STATE_EVENT_pause_simulation,
    STATE_EVENT_shutdown,
    STATE_EVENT_NONE
} state_event_e;

typedef struct state_handler {
    void (*init)(void *);
    void (*enter)(void*,state_e, state_event_e, const char *requestId);
} state_handler_t;

typedef struct state_machine {
    state_e current_state;
    state_handler_t *handlers;
} state_machine_t;

// Function to initialize the state machine module and start its thread
int StateMachine_Launch(void);

// Function to push events to the state machine (if event_queue is managed internally)
void StateMachine_push_event(state_event_e event, const char *requestId);

// Function to signal shutdown and join the state machine thread
void StateMachine_shutdown(void);
int verif_shutdown(void);
#endif