// #include "common/assert_handler.h"
// #include "common/trace.h"
// #include "app/drive.h"
// #include "app/enemy.h"
// #include "app/line.h"
#include "State_Machine.h"
#include "assert_handler.h"

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#define SOCKET_PATH "/tmp/app.sv_simulator"
#define BUFFER_SIZE 1024

typedef struct
{
    int thread_id;
    int socket_fd;
    char buffer[BUFFER_SIZE];
} ThreadData;


void *thread_task(void *arg)
{
    ThreadData *data = (ThreadData *)arg;
     state_machine_run();

}



int main(void)
{
    // Create Unix domain socket
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }
    pthread_t *threads = malloc( sizeof(pthread_t));

    ThreadData *thread_data = malloc( sizeof(ThreadData));
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
    if (pthread_create(&threads, NULL, thread_task, &thread_data) != 0)
    {
        perror("Failed to create thread");
        goto cleanup;
    }
    printf("Created thread \n" );


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
        if (strstr(buffer, "start_configuration")) {
            printf("Received start   config with config: %s\n", buffer);
            //here i nedd to add the part where i send the event interruption
           
            // Send response back to server
            const char *response = "{\"status\":\"Configuration started successfully\"}";

            if (send(sock, response, strlen(response), 0) < 0) {
                perror("Send failed");
                break;
            }
            printf("Sent response: %s\n", response);
        }


else if (strstr(buffer, "start_simulation")) {
            printf("Received start simulation with config: %s\n", buffer);
            // Here you can add the interruption and config data  to the state machine
            // Send response back to server
            const char *response = "{\"status\":\"Simulation started successfully\"}";

            if (send(sock, response, strlen(response), 0) < 0) {
                perror("Send failed");
                break;
            }
            printf("Sent response: %s\n", response);
        }
        else if (strstr(buffer, "stop_simulation")) {
            printf("Received stop simulation with config: %s\n", buffer);
            // Here you can add the logic to stop the simulation
            // Send response back to server
            const char *response = "{\"status\":\"Simulation stopped successfully\"}";

            if (send(sock, response, strlen(response), 0) < 0) {
                perror("Send failed");
                break;
            }
            printf("Sent response: %s\n", response);
        }
    }



   



cleanup:
free(threads);
free(thread_data);
    
    // Cleanup
    close(sock);
    return 0;

}
   
   
