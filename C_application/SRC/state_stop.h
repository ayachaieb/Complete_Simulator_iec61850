#ifndef STATE_STOP_H
#define STATE_STOP_H
#include "State_Machine.h"


struct state_stop_data {
  
    // Add your state-specific data here
};

void state_stop_init(void *data);
void state_stop_enter(void *data , state_e from, state_event_e event);

#endif // STATE_STOP_H