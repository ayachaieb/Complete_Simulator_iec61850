#include "parser.h" // Include this module's header
#include <stdio.h>  // For fprintf, NULL
#include <stdlib.h> // For strdup, free
#include <string.h> // For strcmp, memset
#include <cjson/cJSON.h>
#include "logger.h" // For logging functions
#include "util.h"   // For SUCCESS, FAIL, LOG_ERROR, LOG_DEBUG
// --- Helper function to free allocated memory for SimulationConfig ---
void freeSimulationConfig(SV_SimulationConfig *config)
{
    if (config)
    {
        if (config->appId)
            free(config->appId);
        if (config->dstMac)
            free(config->dstMac);
        if (config->svInterface)
            free(config->svInterface);
        if (config->svIDs)
            free(config->svIDs);
        if (config->scenarioConfigFile)
            free(config->scenarioConfigFile);
        // Clear the struct members to avoid dangling pointers and indicate freed state
        memset(config, 0, sizeof(SV_SimulationConfig));
    }
}

/**
 * @brief Frees all dynamically allocated memory in an SV_SimulationConfig structure
 * @param config Pointer to the SV_SimulationConfig structure to clean up
 */
void freeSVconfig(SV_SimulationConfig *config)
{
    if (config == NULL)
    {
        return;
    }

    // Free all string fields if they were allocated
    if (config->appId)
    {
        free(config->appId);
        config->appId = NULL;
    }
    if (config->dstMac)
    {
        free(config->dstMac);
        config->dstMac = NULL;
    }
    if (config->svInterface)
    {
        free(config->svInterface);
        config->svInterface = NULL;
    }
    if (config->svIDs)
    {
        free(config->svIDs);
        config->svIDs = NULL;
    }
    if (config->scenarioConfigFile)
    {
        free(config->scenarioConfigFile);
        config->scenarioConfigFile = NULL;
    }
}

void freeGOOSEConfig(GOOSE_SimulationConfig *config)
{
    if (config)
    {
        if (config->gocbRef)
            free(config->gocbRef);
        if (config->datSet)
            free(config->datSet);
        if (config->goID)
            free(config->goID);
        if (config->macAddress)
            free(config->macAddress);
        if (config->interface)
            free(config->interface);
        // Clear the struct members to avoid dangling pointers and indicate freed state
        memset(config, 0, sizeof(GOOSE_SimulationConfig));
    }
}
// --- Main Parsing Function ---
/**
 * @brief Parses a JSON request buffer and extracts the type, data, and requestId.
 *
 * @param buffer The JSON string to parse.
 * @param type_obj_out Pointer to store the type object.
 * @param data_obj_out Pointer to store the data object (config array).
 * @param requestId_out Pointer to store the requestId string.
 * @param json_request_out Pointer to store the root cJSON object (to be freed by caller).
 * @return int Returns SUCCESS on successful parsing, FAIL on error.
 */
int parseRequestConfig(
    const char *buffer,
    cJSON **type_obj_out,
    cJSON **data_obj_out,
    char **requestId_out,
    cJSON **json_request_out // Output parameter for the root cJSON object
)
{
    // Initialize output pointers to NULL for safety
    *type_obj_out = NULL;
    *data_obj_out = NULL;
    *requestId_out = NULL;

    *json_request_out = NULL; // Initialize root JSON pointer

    cJSON *json_request = cJSON_Parse(buffer);
    if (!json_request)
    {
        LOG_ERROR("Parser", "Failed to parse incoming JSON: %s", cJSON_GetErrorPtr());
        return FAIL;
    }
    *json_request_out = json_request; // Store the root object for the caller to delete

    // Extract message type
    cJSON *type_obj = cJSON_GetObjectItemCaseSensitive(json_request, "type");
    if (!type_obj || !cJSON_IsString(type_obj))
    { // Corrected: Use type_obj instead of *
        LOG_ERROR("Parser", "Missing or invalid 'type' field in JSON message");
        return FAIL; // json_request_out is already set, caller will delete it
    }
    *type_obj_out = type_obj; // Pass the type object to the caller

    // Extract the main data object (which contains 'config' array and 'requestId')
    cJSON *data_container_obj = cJSON_GetObjectItemCaseSensitive(json_request, "data");
    if (!data_container_obj || !cJSON_IsObject(data_container_obj))
    {
        LOG_ERROR("Parser", "Missing or invalid 'data' object in JSON message");
        return FAIL;
    }

    // Extract requestId from the data_container_obj
    cJSON *requestId_obj = cJSON_GetObjectItemCaseSensitive(data_container_obj, "requestId");
    if (requestId_obj && cJSON_IsString(requestId_obj))
    {
        *requestId_out = strdup(requestId_obj->valuestring);
        if (!*requestId_out)
        {
            LOG_ERROR("Parser", "Failed to duplicate requestId string: Out of memory");
            return FAIL;
        }
    }
    else
    {
        LOG_DEBUG("Parser", "No 'requestId' found or it's not a string.");
    }

    // Extract the 'config' array from the data_container_obj
    cJSON *config_array_obj = cJSON_GetObjectItemCaseSensitive(data_container_obj, "config");
    if (config_array_obj && cJSON_IsArray(config_array_obj))
    {
        *data_obj_out = config_array_obj; // Pass the config array to the caller if it exists and is an array
        LOG_DEBUG("Parser", "Successfully parsed simulation config.");
    }
    else
    {
        LOG_DEBUG("Parser", "'config' array not found or is invalid in 'data' object. Proceeding without it.");
        *data_obj_out = data_container_obj; // Pass the entire data object if 'config' is not present or invalid
    }

    return SUCCESS;
}

// int parseGOOSEConfig(
//     cJSON **data_obj,
//     GOOSE_SimulationConfig *config)
// {
//     if (!data_obj || !*data_obj || !config)
//     {
//         LOG_ERROR("Parser", "Invalid input parameters for GOOSE config parsing");
//         return FAIL;
//     }

//     cJSON *goose_data = *data_obj;

//     // Extract GoCBRef
//     cJSON *GoCBRef_obj = cJSON_GetObjectItemCaseSensitive(goose_data, "gocbRef");
//     if (GoCBRef_obj && cJSON_IsString(GoCBRef_obj))
//     {
//         config->gocbRef = strdup(GoCBRef_obj->valuestring);
//         if (!config->gocbRef)
//         {
//             LOG_ERROR("Parser", "Failed to duplicate GoCBRef string: Out of memory");
//             return FAIL;
//         }
//     }
//     else
//     {
//         LOG_ERROR("Parser", "Missing or invalid 'GoCBRef' field in GOOSE data");
//         return FAIL;
//     }

//     // Extract DatSet
//     cJSON *DatSet_obj = cJSON_GetObjectItemCaseSensitive(goose_data, "datSet");
//     if (DatSet_obj && cJSON_IsString(DatSet_obj))
//     {
//         config->datSet = strdup(DatSet_obj->valuestring);
//         if (!config->datSet)
//         {
//             LOG_ERROR("Parser", "Failed to duplicate DatSet string: Out of memory");
//             freeGOOSEConfig(config); // Free already allocated memory
//             return FAIL;
//         }
//     }
//     else
//     {
//         LOG_ERROR("Parser", "Missing or invalid 'DatSet' field in GOOSE data");
//         freeGOOSEConfig(config); // Free already allocated memory
//         return FAIL;
//     }

//     // Extract GoID
//     cJSON *GoID_obj = cJSON_GetObjectItemCaseSensitive(goose_data, "goID");
//     if (GoID_obj && cJSON_IsString(GoID_obj))
//     {
//         config->goID = strdup(GoID_obj->valuestring);
//         if (!config->goID)
//         {
//             LOG_ERROR("Parser", "Failed to duplicate GoID string: Out of memory");
//             freeGOOSEConfig(config); // Free already allocated memory
//             return FAIL;
//         }
//         return SUCCESS; // Successfully parsed all required fields
//     }
//     else
//     {
//         LOG_ERROR("Parser", "Missing or invalid 'GoID' field in GOOSE data");
//         freeGOOSEConfig(config); // Free already allocated memory
//         return FAIL;
//     }
// }

int parseSVconfig(cJSON *instance_json_obj, SV_SimulationConfig *config_out)
{
    // Validate input parameters
    if (!instance_json_obj || !config_out)
    {
        LOG_ERROR("Parser", "Invalid arguments: %s",
                  !instance_json_obj ? "NULL instance_json_obj" : "NULL config_out");
        return FAIL;
    }

    // Ensure the input is an object
    if (!cJSON_IsObject(instance_json_obj))
    {
        LOG_ERROR("Parser", "Input data_obj is not a JSON object");
        return FAIL;
    }

    // Initialize all pointers to NULL for safe cleanup
    memset(config_out, 0, sizeof(SV_SimulationConfig));

// Helper macro for string field parsing (with error handling)
#define PARSE_STRING_FIELD(field, field_name)                                          \
    do                                                                                 \
    {                                                                                  \
        cJSON *item = cJSON_GetObjectItemCaseSensitive(instance_json_obj, field_name); \
        if (!item || !cJSON_IsString(item))                                            \
        {                                                                              \
            LOG_ERROR("Parser", "Missing or invalid '" field_name "'");                \
            goto cleanup;                                                              \
        }                                                                              \
        config_out->field = strdup(item->valuestring);                                 \
        if (!config_out->field)                                                        \
        {                                                                              \
            LOG_ERROR("Parser", "Memory allocation failed for '" field_name "'");      \
            goto cleanup;                                                              \
        }                                                                              \
    } while (0)

    // Parse required fields based on the new JSON structure
    PARSE_STRING_FIELD(appId, "appId");
    PARSE_STRING_FIELD(dstMac, "dstMac");
    PARSE_STRING_FIELD(svInterface, "svInterface");
    PARSE_STRING_FIELD(scenarioConfigFile, "scenarioConfigFile");
    PARSE_STRING_FIELD(svIDs, "svIDs");
    PARSE_STRING_FIELD(GoCBRef, "GoCBRef");
    PARSE_STRING_FIELD(DatSet, "DatSet");
    PARSE_STRING_FIELD(GoID, "GoID");
    PARSE_STRING_FIELD(MACAddress, "MACAddress");
    PARSE_STRING_FIELD(GOAppID, "AppID");
    PARSE_STRING_FIELD(Interface, "Interface");

#undef PARSE_STRING_FIELD

    return SUCCESS; // Success case

cleanup:
    // Clean up on failure
    // Assuming SV_SimulationConfig has these fields as char* that need freeing
    free(config_out->appId);
    free(config_out->dstMac);
    free(config_out->svInterface);
    free(config_out->scenarioConfigFile);
    free(config_out->svIDs);
    free(config_out->GoCBRef);  
    free(config_out->DatSet);
    free(config_out->GoID);
    free(config_out->MACAddress);
    free(config_out->GOAppID);
    free(config_out->Interface);
    
    memset(config_out, 0, sizeof(SV_SimulationConfig)); // Clear the struct
    return FAIL;
}

void freeIPCConfig(GOOSE_SimulationConfig *config)
{
    if (config)
    {
        free(config->gocbRef);
        free(config->datSet);
        free(config->goID);
        free(config->macAddress);
        free(config->appID);
        free(config->interface);
        free(config->requestId); // Free requestId
        memset(config, 0, sizeof(GOOSE_SimulationConfig)); // Clear the struct after freeing
    }
}

int parseGOOSEConfig(cJSON *IPCdata_obj, GOOSE_SimulationConfig *config_out)
{
    // Validate input parameters
    if (!IPCdata_obj || !config_out)
    {
        LOG_ERROR("Parser", "Invalid arguments: %s",
                  !IPCdata_obj ? "NULL IPCdata_obj" : "NULL config_out");
        return FAIL;
    }

    // Ensure the input is an object
    if (!cJSON_IsObject(IPCdata_obj))
    {
        LOG_ERROR("Parser", "Input IPCdata_obj is not a JSON object");
        return FAIL;
    }

    // Initialize all pointers to NULL for safe cleanup
    memset(config_out, 0, sizeof(GOOSE_SimulationConfig));

    cJSON *config_json_obj = NULL;
    cJSON *request_id_item = NULL;

    // Helper macro for string field parsing (with error handling)
    // This version takes an additional cJSON object to search within
#define PARSE_STRING_FIELD_FROM_OBJ(parent_json_obj, field, field_name)            \
    do                                                                             \
    {                                                                              \
        cJSON *item = cJSON_GetObjectItemCaseSensitive(parent_json_obj, field_name); \
        if (!item || !cJSON_IsString(item))                                        \
        {                                                                          \
            LOG_ERROR("Parser", "Missing or invalid '%s' in %s", field_name, #parent_json_obj); \
            goto cleanup;                                                          \
        }                                                                          \
        config_out->field = strdup(item->valuestring);                             \
        if (!config_out->field)                                                    \
        {                                                                          \
            LOG_ERROR("Parser", "Memory allocation failed for '%s'", field_name); \
            goto cleanup;                                                          \
        }                                                                          \
    } while (0)

    // First, get the "config" object
    config_json_obj = cJSON_GetObjectItemCaseSensitive(IPCdata_obj, "config");
    if (!config_json_obj || !cJSON_IsObject(config_json_obj))
    {
        LOG_ERROR("Parser", "Missing or invalid 'config' object");
        goto cleanup;
    }

    // Parse fields within the "config" object
    PARSE_STRING_FIELD_FROM_OBJ(config_json_obj, gocbRef, "gocbRef");
    PARSE_STRING_FIELD_FROM_OBJ(config_json_obj, datSet, "datSet");
    PARSE_STRING_FIELD_FROM_OBJ(config_json_obj, goID, "goID");
    PARSE_STRING_FIELD_FROM_OBJ(config_json_obj, macAddress, "macAddress");
    PARSE_STRING_FIELD_FROM_OBJ(config_json_obj, appID, "appID");
    PARSE_STRING_FIELD_FROM_OBJ(config_json_obj, interface, "interface");

    // Parse the top-level "requestId"
    request_id_item = cJSON_GetObjectItemCaseSensitive(IPCdata_obj, "requestId");
    if (!request_id_item || !cJSON_IsString(request_id_item))
    {
        LOG_ERROR("Parser", "Missing or invalid 'requestId'");
        goto cleanup;
    }
    config_out->requestId = strdup(request_id_item->valuestring);
    if (!config_out->requestId)
    {
        LOG_ERROR("Parser", "Memory allocation failed for 'requestId'");
        goto cleanup;
    }

#undef PARSE_STRING_FIELD_FROM_OBJ

    return SUCCESS; // Success case

cleanup:
    // Clean up on failure
    freeIPCConfig(config_out); // Use the helper function to free all allocated members
    return FAIL;
}
