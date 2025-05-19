#include "state_init.h"
//#include "common/trace.h"
#include "enum_to_string.h"
#include <stdio.h>
void state_init_init(void *data)
{
    // Initialize init state data here
}

void state_init_enter(void *data ,state_e from, state_event_e event)
{
    printf("Entered INIT state from %s due to %s", state_to_string(from), state_event_to_string(event));
    // Add your INIT state entry logic  here
}