#ifndef STATE_RUNNING_H
#define STATE_RUNNING_H

#include "state_common.h"

struct state_running_data {
    struct state_common_data *common;
    // Add your state-specific data here
};

void state_running_init(struct state_running_data *data);
void state_running_enter(struct state_running_data *data, state_e from, state_event_e event);

#endif // STATE_RUNNING_H