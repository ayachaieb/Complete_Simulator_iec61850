#ifndef ENUM_TO_STRING_H
#define ENUM_TO_STRING_H
#include "State_Machine.h"



/* Functions to convert enum to readable string. String consume flash space, and
 * keeping these in one place makes it easy to compile them out. */

#ifdef DISABLE_ENUM_STRINGS
#include <stdint.h>
#include "common/defines.h"
static inline const char *empty_func(uint8_t val)
{
    UNUSED(val);
    return "";
}


#define state_to_string(state) empty_func(state)
#define state_event_to_string(event) empty_func(event)
#else
const char *state_event_to_string(state_event_e event);
const char *state_to_string(state_e state);
#endif

#endif // ENUM_TO_STRING_H