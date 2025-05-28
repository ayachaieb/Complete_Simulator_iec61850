#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "State_Machine.h"
#define QUEUE_SIZE 16
// Circular buffer (FIFO) that can store elements of any size.

typedef struct {
    state_event_e events[QUEUE_SIZE];
    int head;
    int tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int shutdown;
} EventQueue;

// Initialize event queue
void event_queue_init( EventQueue* event_queue);
// Push event to queue
void event_queue_push(state_event_e event, EventQueue* event_queue);
state_event_e event_queue_pop(EventQueue* event_queue);
#endif // RING_BUFFER_H