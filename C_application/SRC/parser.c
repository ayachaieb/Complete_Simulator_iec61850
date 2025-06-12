#include "parser.h" // Include this module's header
#include <stdio.h>      // For fprintf, NULL
#include <stdlib.h>     // For strdup, free
#include <string.h>     // For strcmp, memset
#include <cjson/cJSON.h> 
#include "logger.h"    // For logging functions
#include "util.h"      // For SUCCESS, FAIL, LOG_ERROR, LOG_DEBUG
// --- Helper function to free allocated memory for SimulationConfig ---
void freeSimulationConfig(SimulationConfig* config) {
    if (config) {
        if (config->appID) free(config->appID);
        if (config->macAddress) free(config->macAddress);
        if (config->interface) free(config->interface);
        if (config->svid) free(config->svid);
        if (config->scenariofile) free(config->scenariofile);
        // Clear the struct members to avoid dangling pointers and indicate freed state
        memset(config, 0, sizeof(SimulationConfig));
    }
}

// --- Main Parsing Function ---

int parseSimulationConfig(
    const char* buffer,
    cJSON** type_obj_out,
    cJSON** data_obj_out,
    char** requestId_out,
    SimulationConfig* config_out,
    cJSON** json_request_out // Output parameter for the root cJSON object
) {
    // Initialize output pointers to NULL for safety
    *type_obj_out = NULL;
    *data_obj_out = NULL;
    *requestId_out = NULL;
    memset(config_out, 0, sizeof(SimulationConfig)); // Initialize config struct to all zeros/NULLs
    *json_request_out = NULL; // Initialize root JSON pointer

    cJSON *json_request = cJSON_Parse(buffer);
    if (!json_request) {
        LOG_ERROR("Parser", "Failed to parse incoming JSON: %s", cJSON_GetErrorPtr());
        return FAIL;
    }
    *json_request_out = json_request; // Store the root object for the caller to delete

    // Extract message type
    cJSON *type_obj = cJSON_GetObjectItemCaseSensitive(json_request, "type");
    if (!type_obj || !cJSON_IsString(type_obj)) { // Corrected: Use type_obj instead of *
        LOG_ERROR("Parser", "Missing or invalid 'type' field in JSON message");
        return FAIL; // json_request_out is already set, caller will delete it
    }
    *type_obj_out = type_obj; // Pass the type object to the caller

    // Extract data object
    cJSON *data_obj = cJSON_GetObjectItemCaseSensitive(json_request, "data");
    if (!data_obj || !cJSON_IsObject(data_obj)) { // Ensure 'data' is an object
        LOG_ERROR("Parser", "Missing or invalid 'data' object in JSON message");
        return FAIL;
    }
    *data_obj_out = data_obj; // Pass the data object to the caller

    // Extract requestId
    cJSON *requestId_obj = cJSON_GetObjectItemCaseSensitive(data_obj, "requestId");
    if (requestId_obj && cJSON_IsString(requestId_obj)) {
        *requestId_out = strdup(requestId_obj->valuestring);
        if (!*requestId_out) {
            LOG_ERROR("Parser", "Failed to duplicate requestId string: Out of memory");
            return FAIL; // Memory error, caller needs to handle deletion of json_request
        }
    } else {
        LOG_DEBUG("Parser", "No 'requestId' found or it's not a string.");
        // Not an error, requestId is optional or can be handled later.
    }

    // --- Parse the 'config' object for SimulationConfig ---
    cJSON *config_json_obj = cJSON_GetObjectItemCaseSensitive(data_obj, "config");
    if (!config_json_obj || !cJSON_IsObject(config_json_obj)) {
        LOG_ERROR("Parser", "Missing or invalid 'config' object within 'data'.");
        return FAIL; // Not a valid config, caller needs to handle deletion
    }

    // Parse each field of the config
    cJSON *item;

    item = cJSON_GetObjectItemCaseSensitive(config_json_obj, "appID");
    if (item && cJSON_IsString(item)) {
        config_out->appID = strdup(item->valuestring);
        if (!config_out->appID) { LOG_ERROR("Parser", "OOM for appID"); return FAIL; }
    } else { LOG_ERROR("Parser", "Missing or invalid 'appID'."); return FAIL; }

    item = cJSON_GetObjectItemCaseSensitive(config_json_obj, "macAddress");
    if (item && cJSON_IsString(item)) {
        config_out->macAddress = strdup(item->valuestring);
        if (!config_out->macAddress) { LOG_ERROR("Parser", "OOM for macAddress"); return FAIL; }
    } else { LOG_ERROR("Parser", "Missing or invalid 'macAddress'."); return FAIL; }

    item = cJSON_GetObjectItemCaseSensitive(config_json_obj, "interface");
    if (item && cJSON_IsString(item)) {
        config_out->interface = strdup(item->valuestring);
        if (!config_out->interface) { LOG_ERROR("Parser", "OOM for interface"); return FAIL; }
    } else { LOG_ERROR("Parser", "Missing or invalid 'interface'."); return FAIL; }

    item = cJSON_GetObjectItemCaseSensitive(config_json_obj, "svid");
    if (item && cJSON_IsString(item)) {
        config_out->svid = strdup(item->valuestring);
        if (!config_out->svid) { LOG_ERROR("Parser", "OOM for svid"); return FAIL; }
    } else { LOG_ERROR("Parser", "Missing or invalid 'svid'."); return FAIL; }

    item = cJSON_GetObjectItemCaseSensitive(config_json_obj, "scenariofile");
    if (item && cJSON_IsString(item)) {
        config_out->scenariofile = strdup(item->valuestring);
        if (!config_out->scenariofile) { LOG_ERROR("Parser", "OOM for scenariofile"); return FAIL; }
    } else { LOG_ERROR("Parser", "Missing or invalid 'scenariofile'."); return FAIL; }

    LOG_DEBUG("Parser", "Successfully parsed simulation config.");
    return SUCCESS;
}