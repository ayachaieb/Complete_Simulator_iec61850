// Example: state_idle.c
#include "../INC/state_idle.h"
//#include "common/trace.h"
#include "../INC/enum_to_string.h"
#include <stdio.h>
void state_idle_init(void *data)
{
    
    // Initialize idle state data here
}

void state_idle_enter( void *data,state_e from, state_event_e event)
{
   // TRACE("Entered IDLE state from %s due to %s", state_to_string(from), state_event_to_string(event));
    printf("Entered IDLE state from %s due to %s", state_to_string(from), state_event_to_string(event));
    // IDLE state entry logic here
}