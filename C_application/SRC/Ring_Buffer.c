#include "Ring_Buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // Add this for strdup()
#include <pthread.h>
#include "util.h"
#include "logger.h"

int event_queue_init(EventQueue* event_queue)
{   
    int  retval = SUCCESS;
    if (pthread_mutex_init(&event_queue->mutex, NULL) != EXIT_SUCCESS) {
        LOG_ERROR("RingBuffer", "pthread_mutex_init() error");
        retval = FAIL;
    }
    else {
        if (pthread_cond_init(&event_queue->cond, NULL) != EXIT_SUCCESS)
        {                                    
            LOG_ERROR("RingBuffer", "pthread_cond_init() error");                                        
            retval = FAIL;
        }
        else 
        {
            event_queue->head = 0;
            event_queue->tail = 0;
            event_queue->shutdown = 0; // Initialize shutdown flag to false
            memset(event_queue->events, 0, sizeof(event_queue->events));
            memset(event_queue->requestIds, 0, sizeof(event_queue->requestIds));
        }
    
    }      
    return retval;                                    
}   

// Push event to queue
int event_queue_push(state_event_e event, const char *requestId, EventQueue* event_queue)
{   
    int retval = SUCCESS;
    pthread_mutex_lock(&event_queue->mutex);
    if ((event_queue->tail + 1) % QUEUE_SIZE != event_queue->head) {
        event_queue->events[event_queue->tail] = event;
        if (NULL != requestId) {
            event_queue->requestIds[event_queue->tail] = strdup(requestId);
            if (event_queue->requestIds[event_queue->tail] == NULL) {
                LOG_ERROR("RingBuffer", "strdup failed for requestId. Event might be pushed without requestId");
                event_queue->requestIds[event_queue->tail] = NULL;
                pthread_mutex_unlock(&event_queue->mutex);
                retval = FAIL;
            }
        } else {
            LOG_WARN("RingBuffer", "NULL requestId provided, storing NULL in queue");
            event_queue->requestIds[event_queue->tail] = NULL;
            pthread_mutex_unlock(&event_queue->mutex);
            retval = FAIL;
        }

        event_queue->tail = (event_queue->tail + 1) % QUEUE_SIZE;
        pthread_cond_signal(&event_queue->cond);
    } else {
        LOG_ERROR("RingBuffer", "Event queue full, dropping event");
        pthread_mutex_unlock(&event_queue->mutex);
        retval = FAIL;
    }
    pthread_mutex_unlock(&event_queue->mutex);
    return retval; 
}

// Pop event from queue 
int event_queue_pop(EventQueue* event_queue, state_event_e* event, const char **requestId_out)
{
    int retval = SUCCESS;
    pthread_mutex_lock(&event_queue->mutex);
    while (event_queue->head == event_queue->tail && !event_queue->shutdown) {
        pthread_cond_wait(&event_queue->cond, &event_queue->mutex);
    }
    if (event_queue->shutdown == 1 && event_queue->head == event_queue->tail) {
        pthread_mutex_unlock(&event_queue->mutex);
        *event = STATE_EVENT_shutdown; 
        retval = SUCCESS; 
    }

    *event = event_queue->events[event_queue->head];
    const char *popped_requestId = event_queue->requestIds[event_queue->head];
    if (requestId_out) {
        *requestId_out = popped_requestId; 
    } else {
        LOG_WARN("RingBuffer", "NULL requestId_out pointer provided, not returning requestId");
        if (popped_requestId) {
            free((char*)popped_requestId);
            event_queue->requestIds[event_queue->head] = NULL;
        }
        pthread_mutex_unlock(&event_queue->mutex);
        retval = FAIL; 
    }
    event_queue->head = (event_queue->head + 1) % QUEUE_SIZE;
    pthread_mutex_unlock(&event_queue->mutex);
    return retval;
}