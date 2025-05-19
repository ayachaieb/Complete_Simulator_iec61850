#include "State_Machine.h"
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/app.sv_simulator"
#define BUFFER_SIZE 1024

// Thread-safe event queue
#define QUEUE_SIZE 16
typedef struct {
    state_event_e events[QUEUE_SIZE];
    int head;
    int tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int shutdown;
} EventQueue;

EventQueue event_queue = { .head = 0, .tail = 0, .shutdown = 0 };

state_machine_t sm_data = { .current_state = STATE_IDLE, .handlers = NULL };

// Initialize event queue
void event_queue_init(void)
{
    pthread_mutex_init(&event_queue.mutex, NULL);
    pthread_cond_init(&event_queue.cond, NULL);
}

// Push event to queue
void event_queue_push(state_event_e event)
{
    pthread_mutex_lock(&event_queue.mutex);
    if ((event_queue.tail + 1) % QUEUE_SIZE != event_queue.head) {
        event_queue.events[event_queue.tail] = event;
        event_queue.tail = (event_queue.tail + 1) % QUEUE_SIZE;
        pthread_cond_signal(&event_queue.cond);
    } else {
        fprintf(stderr, "Event queue full, dropping event\n");
    }
    pthread_mutex_unlock(&event_queue.mutex);
}

// Pop event from queue (blocks if empty)
state_event_e event_queue_pop(void)
{
    pthread_mutex_lock(&event_queue.mutex);
    while (event_queue.head == event_queue.tail && !event_queue.shutdown) {
        pthread_cond_wait(&event_queue.cond, &event_queue.mutex);
    }
    if (event_queue.shutdown && event_queue.head == event_queue.tail) {
        pthread_mutex_unlock(&event_queue.mutex);
        return STATE_EVENT_shutdown;
    }
    state_event_e event = event_queue.events[event_queue.head];
    event_queue.head = (event_queue.head + 1) % QUEUE_SIZE;
    pthread_mutex_unlock(&event_queue.mutex);
    return event;
}

// State machine thread
void *state_machine_thread(void *arg)
{
    state_machine_t *sm = (state_machine_t *)arg;
    if (!sm) {
        fprintf(stderr, "State machine pointer is NULL\n");
        return NULL;
    }
    state_machine_init(sm);
    while (1) {
        state_event_e event = event_queue_pop();
        if (event == STATE_EVENT_shutdown) {
            break;
        }
        state_machine_run(sm, event);
    }
    state_machine_free(sm);
    return NULL;
}

int main(void)
{
    // Initialize event queue
    event_queue_init();

    // Create state machine thread
    pthread_t sm_thread;
    if (pthread_create(&sm_thread, NULL, state_machine_thread, &sm_data) != 0) {
        perror("Failed to create state machine thread");
        return 1;
    }
    printf("Created state machine thread\n");

    // Create Unix domain socket
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        event_queue.shutdown = 1;
        pthread_cond_signal(&event_queue.cond);
        pthread_join(sm_thread, NULL);
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
        event_queue.shutdown = 1;
        pthread_cond_signal(&event_queue.cond);
        pthread_join(sm_thread, NULL);
        return 1;
    }
    printf("Connected to Node.js IPC server\n");

    // Buffer for receiving data
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

        // Map socket messages to state machine events
        state_event_e event = STATE_EVENT_NONE;
        const char *response = NULL;

        if (strstr(buffer, "start_configuration")) {
            printf("Received start config: %s\n", buffer);
            event = STATE_EVENT_start_config;
            response = "{\"status\":\"Configuration started successfully\"}";
        } else if (strstr(buffer, "start_simulation")) {
            printf("Received start simulation: %s\n", buffer);
            event = STATE_EVENT_start_simulation;
            response = "{\"status\":\"Simulation started successfully\"}";
        } else if (strstr(buffer, "stop_simulation")) {
            printf("Received stop simulation: %s\n", buffer);
            event = STATE_EVENT_stop_simulation;
            response = "{\"status\":\"Simulation stopped successfully\"}";
        } else if (strstr(buffer, "pause_simulation")) {
            printf("Received pause simulation: %s\n", buffer);
            event = STATE_EVENT_pause_simulation;
            response = "{\"status\":\"Simulation paused successfully\"}";
        } else if (strstr(buffer, "shutdown")) {
            printf("Received shutdown: %s\n", buffer);
            event = STATE_EVENT_shutdown;
            response = "{\"status\":\"Shutdown initiated\"}";
        }

        // Send event to state machine
        if (event != STATE_EVENT_NONE) {
            event_queue_push(event);
        }

        // Send response back to server
        if (response && send(sock, response, strlen(response), 0) < 0) {
            perror("Send failed");
            break;
        }
        if (response) {
            printf("Sent response: %s\n", response);
        }
    }

    // Cleanup
    event_queue.shutdown = 1;
    pthread_cond_signal(&event_queue.cond);
    pthread_join(sm_thread, NULL);
    close(sock);
    pthread_mutex_destroy(&event_queue.mutex);
    pthread_cond_destroy(&event_queue.cond);
    return 0;
}