#ifndef STATE_RUNNING_H
#define STATE_RUNNING_H

#include "State_Machine.h"

struct state_running_data {
    char data_running[5];
    // Add your state-specific data here
};

void state_running_init(void *data);
void state_running_enter(void *data, state_e from, state_event_e event);

#endif // STATE_RUNNING_H