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
#include <cjson/cJSON.h> // For cJSON parsing
#define SOCKET_PATH "/var/run/app.sv_simulator"
#define BUFFER_SIZE 2048
#define MAX_JSON_SIZE 65536 // Maximum expected JSON message size
static int sock_fd = FAIL;
static struct sockaddr_un server_addr;

int is_complete_json(const char *buffer)
{
    int brace_count = 0;
    int in_string = 0;
    for (int i = 0; buffer[i] != '\0'; i++)
    {
        if (buffer[i] == '\"')
        {
            in_string = !in_string;
        }
        else if (!in_string)
        {
            if (buffer[i] == '{')
            {
                brace_count++;
            }
            else if (buffer[i] == '}')
            {
                brace_count--;
            }
        }
    }
    return brace_count == 0 && strchr(buffer, '{') != NULL && strchr(buffer, '}') != NULL;
}

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

        if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
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
            RetVal = SUCCESS;
        }
    }

    return RetVal;
}

int receive_full_json_message(int sock_fd, char *full_buffer, size_t max_size)
{
    size_t total_received = 0;
    char temp_buffer[BUFFER_SIZE];
    int retval = FAIL;

    // Initialize full_buffer
    full_buffer[0] = '\0';

    while (total_received < max_size)
    {
        ssize_t n = recv(sock_fd, temp_buffer, BUFFER_SIZE - 1, 0);

        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // No data available, try again or break if no data for a while
                // For simplicity, we'll break here. In a real app, you might want a timeout.
                break;
            }
            LOG_ERROR("IPC", "Receive failed: %s", strerror(errno));
            return FAIL;
        }

        if (n == 0)
        {
            LOG_INFO("IPC", "Disconnected from server");
            retval = SUCCESS; // Or handle as an error if an incomplete message was expected
            break;
        }

        temp_buffer[n] = '\0'; // Null-terminate the received chunk

        // Check if adding this chunk would exceed max_size
        if (total_received + n >= max_size)
        {
            LOG_ERROR("IPC", "Received message exceeds MAX_JSON_SIZE");
            return FAIL; // Message too large
        }

        strcat(full_buffer, temp_buffer);
        total_received += n;

        // Check if we have a complete JSON object
        if (is_complete_json(full_buffer))
        {
            // LOG_INFO("IPC", "Received complete JSON: %s", full_buffer);
            return SUCCESS;
        }
    }
}



int ipc_run_loop(int (*shutdown_check_func)(void))
{
    int retval = FAIL;
    if (sock_fd < 0)
    {
        LOG_ERROR("IPC", "Socket not initialized");
        retval = FAIL;
    }
    else
    {

        char full_json_buffer[MAX_JSON_SIZE] = {0}; // Declare a buffer large enough for full JSON messages

        while (!internal_shutdown_flag)
        {
            // Check for shutdown request
            if (shutdown_check_func && shutdown_check_func())
            {
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
                .tv_usec = 100000 // 100ms timeout
            };

            int select_result = select(sock_fd + 1, &read_fds, NULL, NULL, &tv);

            if (select_result == -1)
            {
                if (errno == EINTR)
                {
                    continue; // Interrupted by signal, try again
                }
                LOG_ERROR("IPC", "Select failed: %s", strerror(errno));
                break;
            }

            if (select_result == 0)
            {
                continue; // Timeout, no data available
            }

            // Data is available to read, call receive_full_json_message
            if (receive_full_json_message(sock_fd, full_json_buffer, sizeof(full_json_buffer)) == FAIL)
            {
                LOG_ERROR("IPC", "Failed to receive full JSON message");
                // Depending on error, you might want to continue or break
                continue;
            }

            // Write the full_json_buffer content to a file for verification
            FILE *fp = fopen("received_json.txt", "w");
            if (fp != NULL)
            {
                fprintf(fp, "%s", full_json_buffer);
                fclose(fp);
            //    LOG_INFO("IPC", "Full JSON message written to received_json.txt");
            }
            else
            {
                LOG_ERROR("IPC", "Failed to open received_json.txt for writing");
            }
         
          //  LOG_INFO("IPC", "Received: %s", full_json_buffer);
            // Process the received JSON message
            state_event_e event = STATE_EVENT_NONE;
            char *requestId = NULL;
            cJSON *type_obj, *data_obj;
            // SV_SimulationConfig config = {0}; // Initialize the config struct to zero
            cJSON *json_request = NULL; // This will be set in the parse function

            if (parseRequestConfig(full_json_buffer, &type_obj, &data_obj, &requestId, &json_request) == FAIL)
            {
                LOG_ERROR("IPC", "Failed to parse incoming JSON: %s", cJSON_GetErrorPtr());
                continue;
            }

            // Process event type
            char *event_type = type_obj->valuestring;
            if (strcmp(event_type, "start_simulation") == VALID)
            {
                event = STATE_EVENT_start_simulation;
                LOG_INFO("IPC", "Event: start_simulation");
            }
            else if (strcmp(event_type, "pause_simulation") == VALID)
            {
                event = STATE_EVENT_pause_simulation;
                LOG_INFO("IPC", "Event: pause_simulation");
            }
            else if (strcmp(event_type, "stop_simulation") == VALID)
            {
                event = STATE_EVENT_stop_simulation;
                LOG_INFO("IPC", "Event: stop_simulation");
            }
            else if (strcmp(event_type, "init_success") == VALID)
            {
                event = STATE_EVENT_init_success;
                LOG_INFO("IPC", "Event: init_success");
            }
            else if (strcmp(event_type, "init_failed") == VALID)
            {
                event = STATE_EVENT_init_failed;
                LOG_INFO("IPC", "Event: init_failed");
            }
            else if (strcmp(event_type, "shutdown") == VALID)
            {
                event = STATE_EVENT_shutdown;
                LOG_INFO("IPC", "Event: shutdown");
            }
            else if (strcmp(event_type, "send_goose_message") == VALID)
            {
                event = STATE_EVENT_send_goose;
                printf("IPC", "Event: send_goose");
                LOG_INFO("IPC", "Event: send_goose");
            }
            else if (strcmp(event_type, "receive_goose") == VALID)
            {
                event = STATE_EVENT_send_goose;
                LOG_INFO("IPC", "Event: receive_goose");
            }
            else
            {
                LOG_WARN("IPC", "Unknown event type: %s", event_type);
                continue;
            }

            StateMachine_push_event(event, requestId, data_obj);
            cJSON_Delete(json_request);
            // Free allocated memory
            if (requestId)
            {
                free(requestId);
                requestId = NULL;
            }
        }
    }
    return retval;
}

int ipc_shutdown(void)
{
    int status = EXIT_SUCCESS;

    LOG_INFO("IPC", "Shutting down...");
    internal_shutdown_flag = EXIT_FAILURE;

    if (EXIT_SUCCESS <= sock_fd)
    { // Check if the socket is still open

        if (FAIL == close(sock_fd))
        {
            LOG_ERROR("IPC", "Socket close failed: %s", strerror(errno));
            status = FAIL;
        }
        else
        {
            sock_fd = FAIL;
        }
    }
    return status;
}

int ipc_send_response(const char *response_json)
{
    int retval = FAIL;
    if (sock_fd < 0)
    {
        LOG_ERROR("IPC", "Socket not initialized for sending response");
        retval = FAIL;
    }
    else
    {
        if (!response_json)
        {
            LOG_ERROR("IPC", "Cannot send NULL response");
            retval = FAIL;
        }
        else
        {
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