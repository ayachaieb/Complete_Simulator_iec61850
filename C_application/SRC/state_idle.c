// Example: state_idle.c
#include "state_idle.h"
#include "common/trace.h"

void state_idle_init(struct state_idle_data *data)
{
    // Initialize idle state data here
}

void state_idle_enter(struct state_idle_data *data, state_e from, state_event_e event)
{
    TRACE("Entered IDLE state from %s due to %s", state_to_string(from), state_event_to_string(event));
    // Add your IDLE state entry logic here
}