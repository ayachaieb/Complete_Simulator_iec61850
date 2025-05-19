
#ifndef STATE_INIT_H
#define STATE_INIT_H

#include "State_Machine.h"

typedef struct state_init_data {
    // Add state-specific data if needed
} state_init_data_t;

void state_init_init(void *data);
void state_init_enter(void *data, state_e from, state_event_e event);

#endif