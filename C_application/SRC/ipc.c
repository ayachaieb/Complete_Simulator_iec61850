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

#define SOCKET_PATH "/var/run/app.sv_simulator" 
#define BUFFER_SIZE 1024                

static int sock_fd = FAIL;                     
static struct sockaddr_un server_addr;       
static volatile int internal_shutdown_flag = EXIT_SUCCESS;
static ipc_event_callback_t event_delivery_callback = NULL; 


static void internal_ipc_event_handler(state_event_e event, const char *requestId) 
{
    LOG_DEBUG("ModuleManager", "IPC event handler received event: %d, requestId: %s", 
              event, requestId ? requestId : "N/A");
    StateMachine_push_event(event, requestId);
}

int ipc_init(void) 
{
    int RetVal = SUCCESS;
    // if (!event_cb) {
    //     LOG_ERROR("IPC", "Event callback cannot be NULL");
    //     return FAIL;
    // }
//event_delivery_callback = event_cb;
    // internal_shutdown_flag = 0; 

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) 
    {
        LOG_ERROR("IPC", "Socket creation failed: %s", strerror(errno));
        RetVal = FAIL;
    }
    else
    {
        if (fcntl(sock_fd, F_SETFL, O_NONBLOCK) < 0) //to be removed
        {
            LOG_ERROR("IPC", "Failed to set socket non-blocking: %s", strerror(errno));
            close(sock_fd); 
            sock_fd = FAIL; 
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
            }
        }

    }

    return RetVal; 
}

int ipc_run_loop(int (*shutdown_check_func)(void)) 
{
    char buffer[BUFFER_SIZE]; 

    if (sock_fd < 0) {
        LOG_ERROR("IPC", "Socket not initialized");
        return FAIL;
    }

    while (!internal_shutdown_flag && (shutdown_check_func ? !shutdown_check_func() : 1)) //complexe implementation
    {
        ssize_t n = recv(sock_fd, buffer, BUFFER_SIZE - 1, 0);

        if (EXIT_SUCCESS > n) 
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) 
            {
                usleep(100000); // to be removed
                continue;
            }
            LOG_ERROR("IPC", "Receive failed: %s", strerror(errno));
            return FAIL;
        }
        if (EXIT_SUCCESS == n) 
        {
            LOG_INFO("IPC", "Disconnected from server");
            return EXIT_SUCCESS; 
        }
        
        buffer[n] = '\0';
        LOG_DEBUG("IPC", "Received: %s", buffer);

        state_event_e event = STATE_EVENT_NONE;
        char *requestId_val = NULL;

        cJSON *json_request = cJSON_Parse(buffer);
        if (!json_request) {
            LOG_ERROR("IPC", "Failed to parse incoming JSON: %s", cJSON_GetErrorPtr());
            continue;
        }

        cJSON *type_obj = cJSON_GetObjectItem(json_request, "type");
        cJSON *data_obj = cJSON_GetObjectItem(json_request, "data");
        cJSON *requestId_obj = data_obj ? cJSON_GetObjectItem(data_obj, "requestId") : NULL;

        if (requestId_obj && cJSON_IsString(requestId_obj)) {
            requestId_val = strdup(requestId_obj->valuestring);
            if (!requestId_val) {
                LOG_ERROR("IPC", "Failed to duplicate requestId string");
            }
        }

        if (type_obj && cJSON_IsString(type_obj)) {
            const char* event_type = type_obj->valuestring;
            
            if (strcmp(event_type, "start_simulation") == EXIT_SUCCESS) {
                event = STATE_EVENT_start_simulation;
                LOG_DEBUG("IPC", "Event: start_simulation");
            } 
            else if (strcmp(event_type, "pause_simulation") == EXIT_SUCCESS) {
                event = STATE_EVENT_pause_simulation;
            }
            else if (strcmp(event_type, "stop_simulation") == EXIT_SUCCESS) {
                event = STATE_EVENT_stop_simulation;
            } 
            else if (strcmp(event_type, "init_success") == EXIT_SUCCESS) {
                event = STATE_EVENT_init_success;
            } 
            else if (strcmp(event_type, "init_failed") == EXIT_SUCCESS) {
                event = STATE_EVENT_init_failed;
            } 
            else if (strcmp(event_type, "shutdown") == EXIT_SUCCESS) {
                event = STATE_EVENT_shutdown;
            } 
            else {
                LOG_WARN("IPC", "Unknown event type: %s", event_type);
                cJSON_Delete(json_request);
                continue;
                //remove the continue
            }
        }

        cJSON_Delete(json_request);

        // if (event_delivery_callback) {
        //     event_delivery_callback(event, requestId_val);
        // }
internal_ipc_event_handler(event, requestId_val);

        if (requestId_val) {
            free(requestId_val);
        }
    }
    return EXIT_SUCCESS; 
}

int ipc_shutdown(void) {
    int status = EXIT_SUCCESS;

    LOG_INFO("IPC", "Shutting down...");
    internal_shutdown_flag = EXIT_FAILURE; 

    if (EXIT_SUCCESS <= sock_fd) {  
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
    if (sock_fd < 0) {
        LOG_ERROR("IPC", "Socket not initialized for sending response");
        return -1;
    }
    if (!response_json) {
        LOG_ERROR("IPC", "Cannot send NULL response");
        return -1;
    }

    usleep(5000000); // 5 seconds delay
    LOG_DEBUG("IPC", "Simulating processing delay before sending response");

    ssize_t sent = send(sock_fd, response_json, strlen(response_json), 0);
    if (sent < 0) {
        LOG_ERROR("IPC", "Send failed: %s", strerror(errno));
        return -1;
    }
    
    LOG_DEBUG("IPC", "Sent response (%zd bytes): %s", sent, response_json);
    return 0;
}