#ifndef ENUM_TO_STRING_H
#define ENUM_TO_STRING_H
#include "State_Machine.h"
typedef enum {
    SUCCESS = 0,
    FAIL = -1,
    ERROR_INVALID_PARAM = -2,
    ERROR_TIMEOUT = -3
} StatusCode;

const char *state_event_to_string(state_event_e event);
const char *state_to_string(state_e state);

#endif

