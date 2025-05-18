// Example: state_idle.h
#ifndef STATE_IDLE_H
#define STATE_IDLE_H

#include "state_common.h"

struct state_idle_data {
    struct state_common_data *common;
    // Add your state-specific data here
};

void state_idle_init(struct state_idle_data *data);
void state_idle_enter(struct state_idle_data *data, state_e from, state_event_e event);

#endif // STATE_IDLE_H