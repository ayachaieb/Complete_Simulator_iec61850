// Example: state_idle.h
#ifndef STATE_IDLE_H
#define STATE_IDLE_H
#include "State_Machine.h"
struct state_idle_data {
    //struct state_common_data *common;
    // Add your state-specific data here
};

void state_idle_init(void *data);
void state_idle_enter(void *data, state_e from, state_event_e event);

#endif // STATE_IDLE_H