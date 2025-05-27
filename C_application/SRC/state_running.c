#include "../INC/state_running.h"
//#include "common/trace.h"
#include "../INC/enum_to_string.h"
#include <stdio.h>
void state_running_init(void *data)
{
    // Initialize running state data here
}

void state_running_enter(void *data, state_e from, state_event_e event)
{
    printf("Entered RUNNING state from %s due to %s", state_to_string(from), state_event_to_string(event));
    // Add your RUNNING state entry logic here
}