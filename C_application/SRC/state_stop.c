#include "state_stop.h"
//#include "common/trace.h"
#include "enum_to_string.h"
#include <stdio.h>
void state_stop_init(void *data)
{
    //implementation
}

void state_stop_enter(void *data , state_e from, state_event_e event)
{
    printf("Entered STOP state from %s due to %s", state_to_string(from), state_event_to_string(event));
    // implementation
}