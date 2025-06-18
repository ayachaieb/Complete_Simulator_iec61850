#ifndef ENUM_TO_STRING_H
#define ENUM_TO_STRING_H
#include "State_Machine.h"
#include <stdint.h>
typedef enum {
    VALID = 0,
    SUCCESS = 0,
    FAIL = -1,
    ERROR_INVALID_PARAM = -2,
    ERROR_TIMEOUT = -3
} StatusCode;

const char *state_event_to_string(state_event_e event);
const char *state_to_string(state_e state);
const int string_to_mac(const char *mac_str, uint8_t *dstAddress);
const int parse_mac_address(const char *mac_str, uint8_t *dstAddress);
#endif

