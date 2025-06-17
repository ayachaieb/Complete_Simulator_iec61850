#include "util.h"
#include "State_Machine.h"
#include <stdint.h>
#ifndef DISABLE_ENUM_STRINGS


const char *state_to_string(state_e state)
{
    switch (state) {
    case STATE_IDLE:
        return "IDLE";
    case STATE_INIT:
        return "INIT";
    case STATE_RUNNING:
        return "RUNNING";
    case STATE_STOP:
        return "STOP";
    }
    return "";
}

const char *state_event_to_string(state_event_e event)
{
    switch (event) {
    case STATE_EVENT_init_success:
        return "init_success";
    case STATE_EVENT_shutdown:
        return "shutdown";
    case STATE_EVENT_start_simulation:
        return "start_simulation";
    case STATE_EVENT_stop_simulation:
        return "stop_simulation";
    case STATE_EVENT_pause_simulation:
        return "pause_simulation";
    case STATE_EVENT_init_failed:
        return "init_failed";
    case STATE_EVENT_NONE:
        return "NONE";
    }
    return "";
}
int string_to_mac(const char *mac_str, uint8_t *dstAddress)
{
    if (!mac_str)
        return -1;

    char temp[18]; // Buffer for MAC string (17 chars + null terminator)
    strncpy(temp, mac_str, sizeof(temp));
    temp[sizeof(temp) - 1] = '\0';

    char *token = strtok(temp, ":");
    int i = 0;
    while (token && i < 6)
    {
        dstAddress[i++] = (uint8_t)strtol(token, NULL, 16);
        token = strtok(NULL, ":");
    }
    return (i == 6) ? 0 : -1; // Success if exactly 6 bytes parsed
}
#endif // DISABLE_ENUM_STRINGS