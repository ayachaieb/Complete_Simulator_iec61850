#include "ipc.h"
#include "State_Machine.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>      
#include <sys/socket.h>  
#include <sys/un.h>     
#include <cjson/cJSON.h> 
#include <fcntl.h>       
#include <errno.h>       
#include "util.h"
#include "parser.h"
#define SOCKET_PATH "/var/run/app.sv_simulator" 
#define BUFFER_SIZE 1024                

static int sock_fd = FAIL;                     
static struct sockaddr_un server_addr;       
static volatile int internal_shutdown_flag = EXIT_SUCCESS;

int ipc_init(void) 
{
    int RetVal = SUCCESS;

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) 
    {
        LOG_ERROR("IPC", "Socket creation failed: %s", strerror(errno));
        RetVal = FAIL;
    }
    else
    {
        
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sun_family = AF_UNIX;            
        strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

        if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) 
            {
                if (errno != EINPROGRESS) 
                {
                    LOG_ERROR("IPC", "Connection failed: %s", strerror(errno));
                    close(sock_fd);
                    
                    sock_fd = FAIL;
                    RetVal = FAIL;
                }
            }
            else
            {
                LOG_INFO("IPC", "Connected to Node.js IPC server (or connection in progress)");
                RetVal= SUCCESS;
            }
        

    }

    return RetVal; 
}


int ipc_run_loop(int (*shutdown_check_func)(void))
{
    if (sock_fd < 0) {
        LOG_ERROR("IPC", "Socket not initialized");
        return FAIL;
    }

    char buffer[BUFFER_SIZE] = {0};
    int retval = FAIL;
    
    while (!internal_shutdown_flag)
    {
        // Check for shutdown request
        if (shutdown_check_func && shutdown_check_func()) {
            LOG_INFO("IPC", "Shutdown requested by application");
            retval = SUCCESS;
            break;
        }

        // Use select to wait for data with timeout
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock_fd, &read_fds);
        
        struct timeval tv = {
            .tv_sec = 0,
            .tv_usec = 100000  // 100ms timeout
        };
        
        int select_result = select(sock_fd + 1, &read_fds, NULL, NULL, &tv);
        
        if (select_result == -1) {
            if (errno == EINTR) {
                continue;  // Interrupted by signal, try again
            }
            LOG_ERROR("IPC", "Select failed: %s", strerror(errno));
            break;
        }
        
        if (select_result == 0) {
            continue;  // Timeout, no data available
        }
        
        // Data is available to read
        ssize_t n = recv(sock_fd, buffer, BUFFER_SIZE - 1, 0);
        //mechanism d erreur handling
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;  // No data available, try again
            }
            LOG_ERROR("IPC", "Receive failed: %s", strerror(errno));
            break;
        }
        
        if (n == 0) {
            LOG_INFO("IPC", "Disconnected from server");
            retval = SUCCESS;
            break;
        }
        
        // Ensure null termination
        buffer[n] = '\0';
        LOG_DEBUG("IPC", "Received: %s", buffer);
        printf("Received: %s\n", buffer); // For debugging purposes
        // Process the received JSON message
        state_event_e event = STATE_EVENT_NONE;
        char *requestId = NULL;
         cJSON *type_obj,*data_obj ;
         SimulationConfig config = {0}; // Initialize the config struct to zero
        cJSON *json_request = NULL; // This will be set in the parse function
        // cJSON *json_request = cJSON_Parse(buffer);
        // if (!json_request) {
        //     LOG_ERROR("IPC", "Failed to parse incoming JSON: %s", cJSON_GetErrorPtr());
        //     continue;
        // }
        
        // // Extract message type
        // cJSON *type_obj = cJSON_GetObjectItem(json_request, "type");
        // if (!type_obj || !cJSON_IsString(type_obj)) {
        //     LOG_ERROR("IPC", "Missing or invalid 'type' field in JSON message");
        //     cJSON_Delete(json_request);
        //     continue;
        // }
        
        // // Extract data and requestId
        // cJSON *data_obj = cJSON_GetObjectItem(json_request, "data");
        // cJSON *requestId_obj = data_obj ? cJSON_GetObjectItem(data_obj, "requestId") : NULL;
        
        // if (requestId_obj && cJSON_IsString(requestId_obj)) {
        //     requestId = strdup(requestId_obj->valuestring);
        //     if (!requestId) {
        //         LOG_ERROR("IPC", "Failed to duplicate requestId string: Out of memory");
        //         cJSON_Delete(json_request);
        //         continue;
        //     }
        // }
        if ( parseSimulationConfig(buffer, &type_obj, &data_obj,&requestId,&config,json_request) == FAIL) 
        {
            LOG_ERROR("IPC", "Failed to parse incoming JSON: %s", cJSON_GetErrorPtr());
            continue;
        }

        // Process event type
        char *event_type = type_obj->valuestring;
        if (strcmp(event_type, "start_simulation") == VALID) {
            event = STATE_EVENT_start_simulation;
            LOG_DEBUG("IPC", "Event: start_simulation");
        }
        else if (strcmp(event_type, "pause_simulation") == VALID) {
            event = STATE_EVENT_pause_simulation;
            LOG_DEBUG("IPC", "Event: pause_simulation");
        }
        else if (strcmp(event_type, "stop_simulation") == VALID) {
            event = STATE_EVENT_stop_simulation;
            LOG_DEBUG("IPC", "Event: stop_simulation");
        }
        else if (strcmp(event_type, "init_success") == VALID) {
            event = STATE_EVENT_init_success;
            LOG_DEBUG("IPC", "Event: init_success");
        }
        else if (strcmp(event_type, "init_failed") == VALID) {
            event = STATE_EVENT_init_failed;
            LOG_DEBUG("IPC", "Event: init_failed");
        }
        else if (strcmp(event_type, "shutdown") == VALID) {
            event = STATE_EVENT_shutdown;
            LOG_DEBUG("IPC", "Event: shutdown");
        }
        else {
            LOG_WARN("IPC", "Unknown event type: %s", event_type);
            continue;
        }
        
        cJSON_Delete(json_request);
        
        // Process the event
        LOG_DEBUG("ModuleManager", "IPC event handler received event: %d, requestId: %s",
                 event, requestId ? requestId : "N/A");
        
        StateMachine_push_event(event, requestId);
        
        // Free allocated memory
        if (requestId) {
            free(requestId);
            requestId = NULL;
        }
    }
    
    return retval;
}

int ipc_shutdown(void) {
    int status = EXIT_SUCCESS;

    LOG_INFO("IPC", "Shutting down...");
    internal_shutdown_flag = EXIT_FAILURE; 

    if (EXIT_SUCCESS <= sock_fd) {  // Check if the socket is still open

        if (FAIL == close(sock_fd)) {  
            LOG_ERROR("IPC", "Socket close failed: %s", strerror(errno));
            status = FAIL; 
        } else {
            sock_fd = FAIL; 
        }
    }
    return status;
}

int ipc_send_response(const char *response_json) {
    int retval = FAIL;
    if (sock_fd < 0) {
        LOG_ERROR("IPC", "Socket not initialized for sending response");
        retval = FAIL;
    }
    else 
    {    if (!response_json) 
        {
        LOG_ERROR("IPC", "Cannot send NULL response");
        retval = FAIL;
        }
        else {
            ssize_t sent = send(sock_fd, response_json, strlen(response_json), 0);
            if (sent < 0) 
            {
            LOG_ERROR("IPC", "Send failed: %s", strerror(errno));
            retval = FAIL;
            }
    
            LOG_DEBUG("IPC", "Sent response (%zd bytes): %s", sent, response_json);
            retval = SUCCESS;

        }

    }
    return retval;
}