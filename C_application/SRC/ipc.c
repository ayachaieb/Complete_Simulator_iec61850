#include "ipc.h"
#include "State_Machine.h"
#include <stdio.h>
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

int ipc_init(ipc_event_callback_t event_cb) {
    if (!event_cb) {
        fprintf(stderr, "ipc_init: Error: event_cb cannot be NULL.\n");
        return FAIL;
    }
    event_delivery_callback = event_cb;
    internal_shutdown_flag = 0; 

   
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("ipc: Socket creation failed");
        return FAIL;
    }


    if (fcntl(sock_fd, F_SETFL, O_NONBLOCK) < 0) {
        perror("ipc: Failed to set socket non-blocking");
        close(sock_fd); 
        sock_fd = FAIL; 
        return FAIL;
    }


    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;            
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        if (errno != EINPROGRESS) {
            perror("ipc: Connection failed");
            close(sock_fd);
            sock_fd = FAIL;
            return FAIL;
        }
    }
  //  printf("ipc: Connected to Node.js IPC server (or connection in progress).\n");
    return EXIT_SUCCESS; 
}

int ipc_run_loop(int (*shutdown_check_func)(void)) {
    char buffer[BUFFER_SIZE]; 

    if (sock_fd < 0) {
        fprintf(stderr, "ipc: Socket not initialized.\n");
        return FAIL;
    }

    
    while (!internal_shutdown_flag && (shutdown_check_func ? !shutdown_check_func() : 1)) {
       
        ssize_t n = recv(sock_fd, buffer, BUFFER_SIZE - 1, 0);

        if (EXIT_SUCCESS > n) {
        
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(100000); 
                continue;       // Continue to the next iteration of the loop
            }
            perror("ipc: Receive failed");
            return FAIL; //  critical error
        }
        if (n == EXIT_SUCCESS) {
            //indicates that the peer has performed an orderly shutdown.
            printf("ipc: Disconnected from server.\n");
            return EXIT_SUCCESS; 
        }
        buffer[n] = '\0'; // Null-terminate the received data
        printf("ipc: Received: %s\n", buffer);
         fflush(stdout); // Ensure this message is written out immediately

        state_event_e event = STATE_EVENT_NONE;
        char *requestId_val = NULL; // To store requestId value 

        // extract 'type' and 'requestId'
        cJSON *json_request = cJSON_Parse(buffer);
        if (!json_request) {
            fprintf(stderr, "ipc: Failed to parse incoming JSON: %s\n", cJSON_GetErrorPtr());
            continue;
        }

        cJSON *type_obj = cJSON_GetObjectItem(json_request, "type");
        cJSON *data_obj = cJSON_GetObjectItem(json_request, "data");
        cJSON *requestId_obj = data_obj ? cJSON_GetObjectItem(data_obj, "requestId") : NULL;

        if (requestId_obj && cJSON_IsString(requestId_obj)) {
            requestId_val = strdup(requestId_obj->valuestring);
            if (!requestId_val) {
                fprintf(stderr, "ipc: Failed to duplicate requestId string.\n");
            }
        }

        if (type_obj && cJSON_IsString(type_obj)) {
            if (strcmp(type_obj->valuestring, "start_simulation") == EXIT_SUCCESS) {
                event = STATE_EVENT_start_simulation;
                printf("ipc: Event: start_simulation\n");
            } else if (strcmp(type_obj->valuestring, "pause_simulation") == EXIT_SUCCESS) {
                event = STATE_EVENT_pause_simulation;
            }
            else if (strcmp(type_obj->valuestring, "stop_simulation") == EXIT_SUCCESS) {
                event = STATE_EVENT_stop_simulation;
            } else if (strcmp(type_obj->valuestring, "init_success") == EXIT_SUCCESS) {
                event = STATE_EVENT_init_success;
            } else if (strcmp(type_obj->valuestring, "init_failed") == EXIT_SUCCESS) {
                event = STATE_EVENT_init_failed;
            } else if (strcmp(type_obj->valuestring, "shutdown") == EXIT_SUCCESS) {
                event = STATE_EVENT_shutdown;
            } else {
                fprintf(stderr, "ipc: Unknown event type: %s\n", type_obj->valuestring);
                cJSON_Delete(json_request);
                continue; // Skip to the next iteration
            }
  
        }

        cJSON_Delete(json_request); 

        // Deliver event and requestId via the callback to the main application logic
        if (event_delivery_callback) {
            event_delivery_callback(event, requestId_val);
        }

        if (requestId_val) {
            free(requestId_val);
        }
    }
    return EXIT_SUCCESS; 
}

int ipc_shutdown(void) {
    int status = EXIT_SUCCESS; // Assume success unless an error occurs

    printf("ipc: Shutting down...\n");
    internal_shutdown_flag = EXIT_FAILURE; 

    if (EXIT_SUCCESS <= sock_fd) {  
        if (FAIL == close(sock_fd) ) {  
            perror("close"); 
            status = FAIL; 
        } else {
            sock_fd = FAIL; 
        }
    }
    return status; // Return success or failure
}

int ipc_send_response(const char *response_json) {
    if (sock_fd < 0) {
        fprintf(stderr, "ipc: Socket not initialized for sending response.\n");
        return -1;
    }
    if (!response_json) {
        fprintf(stderr, "ipc: Cannot send NULL response.\n");
        return -1;
    }

    // Simulate processing delay
    usleep(5000000); // 5 seconds (5,000,000 microseconds)
    printf("ipc: Simulating processing delay before sending response.\n");

    ssize_t sent = send(sock_fd, response_json, strlen(response_json), 0);
    if (sent < 0) {
        perror("ipc: Send failed");
        return -1;
    }
    printf("ipc: Sent response (%zd bytes): %s\n", sent, response_json);
    return 0; // Success
}