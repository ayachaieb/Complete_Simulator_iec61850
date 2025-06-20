#include "util.h"
#include "State_Machine.h"
#include <stdint.h>
#ifndef DISABLE_ENUM_STRINGS

const char *state_to_string(state_e state)
{
    switch (state)
    {
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
    switch (event)
    {
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
// Dummy function for parsing MAC address string to uint8_t array
int parse_mac_address(const char *mac_str, uint8_t mac_array[6])
{
    if (mac_str == NULL)
        return -1;
    int result = sscanf(mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                        &mac_array[0], &mac_array[1], &mac_array[2],
                        &mac_array[3], &mac_array[4], &mac_array[5]);
    return result == 6;
}
#endif // DISABLE_ENUM_STRINGS