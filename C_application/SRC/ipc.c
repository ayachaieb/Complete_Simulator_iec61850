#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cjson/cJSON.h>
//#include "Ring_Buffer.h"
#define BUFFER_SIZE 1024

int ipc_init(const char *socket_path, int *sock) {
    *sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (*sock < 0) {
        perror("Socket creation failed");
        return -1;
    }

    if (fcntl(*sock, F_SETFL, O_NONBLOCK) < 0) {
        perror("Failed to set socket non-blocking");
        close(*sock);
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(*sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        // Handle EADDRINUSE for existing socket files gracefully
        if (errno == EADDRINUSE) {
            fprintf(stderr, "Socket file %s already exists, trying to unlink and reconnect...\n", socket_path);
            unlink(socket_path); // Attempt to remove the stale socket file
            if (connect(*sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                perror("Connection failed after unlink");
                close(*sock);
                return -1;
            }
        } else {
            perror("Connection failed");
            close(*sock);
            return -1;
        }
    }
    printf("Connected to Node.js IPC server at %s\n", socket_path);
    return 0;
}

void ipc_receive_messages(int sock, message_handler_cb handler_cb, EventQueue *event_q) {
    char buffer[BUFFER_SIZE];
    ssize_t n = recv(sock, buffer, BUFFER_SIZE - 1, 0);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available, the caller should handle brief pauses
            return;
        }
        perror("Receive failed");
        // In a real application, you might want to signal a shutdown or error state
        if (event_q) {
            event_q->shutdown = 1;
            pthread_cond_signal(&event_q->cond);
        }
        return;
    }
    if (n == 0) {
        printf("Disconnected from server\n");
        if (event_q) {
            event_q->shutdown = 1;
            pthread_cond_signal(&event_q->cond);
        }
        return;
    }
    buffer[n] = '\0';
    printf("Received: %s\n", buffer);

    cJSON *json_request = cJSON_Parse(buffer);
    if (!json_request) {
        fprintf(stderr, "Failed to parse incoming JSON: %s\n", cJSON_GetErrorPtr());
        return;
    }

    if (handler_cb) {
        handler_cb(json_request, sock, event_q);
    }
    cJSON_Delete(json_request);
}

ssize_t ipc_send_json_response(int sock, cJSON *json_response) {
    char *response_str = cJSON_PrintUnformatted(json_response);
    if (!response_str) {
        fprintf(stderr, "Failed to serialize JSON response\n");
        return -1;
    }

    ssize_t sent = send(sock, response_str, strlen(response_str), 0);
    if (sent < 0) {
        perror("Send failed");
    } else {
        printf("Sent response (%zd bytes): %s\n", sent, response_str);
    }
    free(response_str);
    return sent;
}

void ipc_cleanup(int sock) {
    close(sock);
    // You might also want to unlink the socket path here if this module is the server
    // For a client, typically you don't unlink unless you created the socket file.
}