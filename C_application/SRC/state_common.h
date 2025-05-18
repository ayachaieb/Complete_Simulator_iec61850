#ifndef STATE_COMMON_H
#define STATE_COMMON_H

#include <stdint.h>

// Definitions common between states

typedef enum {
    STATE_IDLE,
    STATE_INIT,
    STATE_RUNNING,
    STATE_STOP 
} state_e;

typedef enum {
    STATE_EVENT_start_config,
    STATE_EVENT_start_simulation,
    STATE_EVENT_stop_simulation,
    STATE_EVENT_pause_simulation,
    STATE_EVENT_shutdown,
    STATE_EVENT_config_failed,
    STATE_EVENT_NONE
} state_event_e;

struct state_machine_data;
typedef uint32_t timer_t;
struct ring_buffer;

struct state_common_data
{
    struct state_machine_data *state_machine_data;
    timer_t *timer;
    struct ring_buffer *input_history;
};

// Post event from inside a state
void state_machine_post_internal_event(struct state_machine_data *data, state_event_e event);

#endif // STATE_COMMON_H