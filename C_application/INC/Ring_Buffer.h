#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "State_Machine.h"
#include <cjson/cJSON.h> 
#include "parser.h" // For SV_SimulationConfig and GOOSE_SimulationConfig
#define QUEUE_SIZE 16
// Circular buffer (FIFO) that can store elements of any size.

typedef struct {
    state_event_e events[QUEUE_SIZE];
    const char *requestIds[QUEUE_SIZE]; 
    cJSON *data_objs[QUEUE_SIZE]; // Array to store cJSON objects associated with events
    int head;
    int tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int shutdown;
} EventQueue;

// Initialize event queue
int event_queue_init( EventQueue* event_queue);
// Push event to queue
int event_queue_push(state_event_e event,const char *requestId, EventQueue* event_queue, cJSON *data_obj);
int event_queue_pop(EventQueue* event_queue,state_event_e* event, const char **requestId, cJSON **data_obj_out);
#endif // RING_BUFFER_H