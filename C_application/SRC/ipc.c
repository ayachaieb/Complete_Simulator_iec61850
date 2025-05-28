#include "ipc.h"
#include "State_Machine.h" // For state_event_e definitions

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>      // For close(), usleep()
#include <sys/socket.h>  // For socket(), connect(), recv(), send()
#include <sys/un.h>      // For sockaddr_un
#include <cjson/cJSON.h> // For JSON parsing
#include <fcntl.h>       // For fcntl(), O_NONBLOCK
#include <errno.h>       // For errno, EAGAIN, EWOULDBLOCK

// Define the path to the Unix domain socket
// IMPORTANT: Ensure this path is accessible by the application.
// For /var directory, ensure appropriate permissions if necessary.
#define SOCKET_PATH "/tmp/app.sv_simulator" // Path to the Unix domain socket
#define BUFFER_SIZE 1024                    // Buffer for receiving data

// Internal static variables for this module, private to this compilation unit
static int sock_fd = -1;                     // File descriptor for the Unix domain socket
static struct sockaddr_un server_addr;       // Structure for the server address
static volatile int internal_shutdown_flag = 0; // Flag to signal the run loop to terminate
static ipc_event_callback_t event_delivery_callback = NULL; // Callback for delivering events

int ipc_init(ipc_event_callback_t event_cb) {
    if (!event_cb) {
        fprintf(stderr, "ipc_init: Error: event_cb cannot be NULL.\n");
        return -1;
    }
    event_delivery_callback = event_cb;
    internal_shutdown_flag = 0; // Ensure flag is reset on init

    // 1. Create Unix domain socket (AF_UNIX for local communication, SOCK_STREAM for reliable, connection-oriented)
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("ipc: Socket creation failed");
        return -1;
    }

    // 2. Set socket to non-blocking mode
    // O_NONBLOCK ensures that read/write operations (like recv, send, connect)
    // return immediately if they cannot complete without blocking, instead of waiting.
    if (fcntl(sock_fd, F_SETFL, O_NONBLOCK) < 0) {
        perror("ipc: Failed to set socket non-blocking");
        close(sock_fd); // Clean up the socket if setting options fails
        sock_fd = -1;   // Invalidate the descriptor
        return -1;
    }

    // 3. Configure server address
    memset(&server_addr, 0, sizeof(server_addr)); // Clear the address structure
    server_addr.sun_family = AF_UNIX;             // Specify Unix domain family
    // Copy the socket path; -1 to ensure null termination
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    // 4. Connect to the server
    // For non-blocking sockets, connect() might return -1 with errno set to EINPROGRESS
    // if the connection cannot be established immediately. This is not an error;
    // it means the connection is in progress.
    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        if (errno != EINPROGRESS) {
            perror("ipc: Connection failed");
            close(sock_fd);
            sock_fd = -1;
            return -1;
        }
        // If EINPROGRESS, connection is asynchronous, we proceed.
        // For production, you might want to use select/poll to wait for connection completion.
    }
    printf("ipc: Connected to Node.js IPC server (or connection in progress).\n");
    return 0; // Initialization successful
}

int ipc_run_loop(int (*shutdown_check_func)(void)) {
    char buffer[BUFFER_SIZE]; // Buffer for incoming data

    if (sock_fd < 0) {
        fprintf(stderr, "ipc: Socket not initialized. Call ipc_init() first.\n");
        return -1;
    }

    // Loop as long as neither the internal shutdown flag nor the external shutdown check is true
    while (!internal_shutdown_flag && (shutdown_check_func ? !shutdown_check_func() : 1)) {
        // Receive data from server (non-blocking)
        ssize_t n = recv(sock_fd, buffer, BUFFER_SIZE - 1, 0);

        if (n < 0) {
            // Check for non-blocking specific errors:
            // EAGAIN or EWOULDBLOCK mean no data is immediately available.
            // This is normal for a non-blocking socket when no data is sent.
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available, sleep briefly to avoid busy-waiting and high CPU usage.
                usleep(100000); // Sleep for 100 milliseconds
                continue;       // Continue to the next iteration of the loop
            }
            // Other negative return values indicate a real error
            perror("ipc: Receive failed");
            return -1; // Indicate a critical error
        }
        if (n == 0) {
            // A return value of 0 from recv() indicates that the peer has performed an orderly shutdown.
            printf("ipc: Disconnected from server.\n");
            return 0; // Graceful disconnection
        }
        buffer[n] = '\0'; // Null-terminate the received data
        printf("ipc: Received: %s\n", buffer);

        state_event_e event = STATE_EVENT_NONE;
        char *requestId_val = NULL; // To store requestId value if found (needs to be free'd)

        // Parse incoming JSON to extract 'type' and 'requestId'
        cJSON *json_request = cJSON_Parse(buffer);
        if (!json_request) {
            fprintf(stderr, "ipc: Failed to parse incoming JSON: %s\n", cJSON_GetErrorPtr());
            // Consider sending an error response back if parsing fails
            continue;
        }

        cJSON *type_obj = cJSON_GetObjectItem(json_request, "type");
        cJSON *data_obj = cJSON_GetObjectItem(json_request, "data");
        cJSON *requestId_obj = data_obj ? cJSON_GetObjectItem(data_obj, "requestId") : NULL;

        if (requestId_obj && cJSON_IsString(requestId_obj)) {
            // Duplicate the string value, as the cJSON object will be deleted
            requestId_val = strdup(requestId_obj->valuestring);
            if (!requestId_val) {
                fprintf(stderr, "ipc: Failed to duplicate requestId string.\n");
            }
        }

        if (type_obj && cJSON_IsString(type_obj)) {
            // Map the incoming message type to a state machine event
            if (strcmp(type_obj->valuestring, "start_simulation") == 0) {
                event = STATE_EVENT_start_simulation;
            } else if (strcmp(type_obj->valuestring, "stop_simulation") == 0) {
                event = STATE_EVENT_stop_simulation;
            } else if (strcmp(type_obj->valuestring, "pause_simulation") == 0) {
                event = STATE_EVENT_pause_simulation;
            }
            // Add more event mappings as needed
            // else if (strcmp(type_obj->valuestring, "resume_simulation") == 0) {
            //     event = STATE_EVENT_resume_simulation;
            // }
        }

        cJSON_Delete(json_request); // Always clean up parsed JSON object

        // Deliver event and requestId via the callback to the main application logic
        if (event_delivery_callback) {
            event_delivery_callback(event, requestId_val);
        }

        if (requestId_val) {
            free(requestId_val); // Free the duplicated requestId string
        }
    }
    return 0; // Loop exited due to shutdown
}

void ipc_shutdown(void) {
    printf("ipc: Shutting down...\n");
    internal_shutdown_flag = 1; // Set the internal flag to terminate the run loop

    if (sock_fd >= 0) {
        close(sock_fd); // Close the socket. This will cause recv() to return 0 or an error.
        sock_fd = -1;   // Invalidate the file descriptor
    }
    printf("Ipc: Shutdown complete.\n");
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

    // Simulate processing delay as in your original code
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