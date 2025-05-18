#ifndef STATE_INIT_H
#define STATE_INIT_H

#include "state_common.h"

struct state_init_data {
    struct state_common_data *common;
    // Add your state-specific data here
};

void state_init_init(struct state_init_data *data);
void state_init_enter(struct state_init_data *data, state_e from, state_event_e event);

#endif // STATE_INIT_H