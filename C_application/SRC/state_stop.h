#ifndef STATE_STOP_H
#define STATE_STOP_H

#include "state_common.h"

struct state_stop_data {
    struct state_common_data *common;
    // Add your state-specific data here
};

void state_stop_init(struct state_stop_data *data);
void state_stop_enter(struct state_stop_data *data, state_e from, state_event_e event);

#endif // STATE_STOP_H