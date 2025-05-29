#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/app.sv_simulator"
#define BUFFER_SIZE 1024

int main() {
    // Create Unix domain socket
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Configure server address
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    // Connect to server
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return 1;
    }

    printf("Connected to Node.js IPC server\n");

    // Buffer for receiving and sending data
    char buffer[BUFFER_SIZE];
    while (1) {
        // Receive data from server
        ssize_t n = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (n <= 0) {
            printf("Disconnected from server\n");
            break;
        }
        buffer[n] = '\0';
        printf("Received: %s\n", buffer);

        // Check for start_simulation message
        if (strstr(buffer, "start_simulation")) {
            printf("Starting SV simulation with config: %s\n", buffer);

            // Send response back to server
            const char *response = "{\"status\":\"Simulation started successfully\"}";
            if (send(sock, response, strlen(response), 0) < 0) {
                perror("Send failed");
                break;
            }
            printf("Sent response: %s\n", response);
        }
    }

    // Cleanup
    close(sock);
    return 0;
}
