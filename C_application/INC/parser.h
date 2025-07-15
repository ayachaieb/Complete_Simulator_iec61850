#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h> // For bool type
#include <cjson/cJSON.h> // For cJSON parsing



typedef struct {
    char *appId;
    char *dstMac;
    char *svInterface;
    char *scenarioConfigFile;
    char *svIDs;
    
    char* GoCBRef; // Reference to the GOOSE Control Block
    char* DatSet;  // Data Set reference
    char* GoID;    // GOOSE ID
    char* MACAddress; // MAC address for GOOSE communication
    char* AppID;  // Application ID for GOOSE
    char* Interface; // Network interface for GOOSE communication
} SV_SimulationConfig;


typedef struct {
    char* GoCBRef; // Reference to the GOOSE Control Block
    char* DatSet;  // Data Set reference
    char* GoID;    // GOOSE ID
    char* MACAddress; // MAC address for GOOSE communication
    char* AppID;  // Application ID for GOOSE
    char* Interface; // Network interface for GOOSE communication
}GOOSE_SimulationConfig;

// --- Parser Interface ---

/**
 * @brief Parses a JSON buffer for simulation configuration data.
 *
 * This function parses the incoming JSON string, extracts the message type,
 * request ID, and the detailed simulation configuration. It allocates memory
 * for duplicated strings within SimulatappIDionConfig and requestId.
 * The caller is responsible for freeing these allocated strings.
 *
 * @param buffer The null-terminated string containing the JSON message.
 * @param type_obj A pointer to a cJSON pointer where the 'type' cJSON object will be stored.
 * The caller should NOT delete this cJSON object directly; it's part of the
 * returned json_request.
 * @param data_obj A pointer to a cJSON pointer where the 'data' cJSON object will be stored.
 * The caller should NOT delete this cJSON object directly; it's part of the
 * returned json_request.
 * @param requestId A pointer to a char pointer where the duplicated requestId string will be stored.
 * The caller is responsible for freeing this memory using free().
 * @param config A pointer to a SV_SimulationConfig struct where the parsed configuration data will be stored.
 * The caller is responsible for freeing the char* members within this struct using free().
 * @param json_request_out A pointer to a cJSON pointer where the root cJSON object parsed from the buffer will be stored.
 * The caller is responsible for freeing this cJSON object using cJSON_Delete().
 * @return SUCCESS (0) on successful parsing and data extraction, FAIL (-1) on error.
 */
int parseRequestConfig(
    const char* buffer,
    cJSON** type_obj,
    cJSON** data_obj,
    char** requestId,
    cJSON** json_request_out // Added to return the root cJSON object
);
int parseGOOSEConfig(
    cJSON** data_obj,
    GOOSE_SimulationConfig* config
);
parseSVconfig(
    cJSON* data_obj,
    SV_SimulationConfig* config
);

// Helper function to free allocated memory in SV_SimulationConfig
// This can be in parser.c if it's strictly internal, or here if external modules need it.
void freeSimulationConfig (SV_SimulationConfig* config);
void freeGOOSEConfig (GOOSE_SimulationConfig* config);
void freeSVconfig(SV_SimulationConfig* config) ;
#endif // PARSER_H