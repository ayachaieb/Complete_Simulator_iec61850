#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>

// Structure to hold instance data from XML
typedef struct {
    uint16_t appId;         // SV appId (hex)
    char *svInterface;      // SV interface (e.g., "enp0s31f6")
    char **svIDs;           // Array of svID strings
    int svIDCount;          // Number of svIDs
    char *gooseInterface;   // GOOSE interface (e.g., "lo")
    uint16_t gooseAppId;    // GOOSE appId (hex)
    uint8_t dstMac[6];      // Destination MAC address
    char *goCbRef;          // GOOSE control block reference
    char *scenarioConfigFile; // Scenario config file path
} Instance;

// Parse XML file into Instance array and sampling time
int parse_xml(const char *filename, Instance **instances, int *instance_count, int *sampling_time);

// Free allocated memory for instances
void free_instances(Instance *instances, int instance_count);

#endif // PARSER_H