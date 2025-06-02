#include "Ring_Buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "util.h"
#include "logger.h"  

int event_queue_init(EventQueue* event_queue)
{
    if ( pthread_mutex_init(&event_queue->mutex, NULL) != EXIT_SUCCESS) {
        LOG_ERROR("RingBuffer", "pthread_mutex_init() error", __FILE__, __LINE__);
        return FAIL;
    }
    if (pthread_cond_init(&event_queue->cond, NULL) != EXIT_SUCCESS) {                                    
        LOG_ERROR("RingBuffer", "pthread_cond_init() error", __FILE__, __LINE__);                                        
        return FAIL;
    }        
    return SUCCESS;                                    
}   

// Push event to queue
int event_queue_push(state_event_e event, const char *requestId, EventQueue* event_queue)
{
    pthread_mutex_lock(&event_queue->mutex);
    if ((event_queue->tail + 1) % QUEUE_SIZE != event_queue->head) {
        event_queue->events[event_queue->tail] = event;

        if (requestId) {
            event_queue->requestIds[event_queue->tail] = strdup(requestId);
            if (event_queue->requestIds[event_queue->tail] == NULL) {
                LOG_ERROR("RingBuffer", "strdup failed for requestId. Event might be pushed without requestId", __FILE__, __LINE__);
                event_queue->requestIds[event_queue->tail] = NULL;
                pthread_mutex_unlock(&event_queue->mutex);
                return FAIL;
            }
        } else {
            LOG_WARN("RingBuffer", "NULL requestId provided, storing NULL in queue", __FILE__, __LINE__);
            event_queue->requestIds[event_queue->tail] = NULL;
            pthread_mutex_unlock(&event_queue->mutex);
            return FAIL;
        }

        event_queue->tail = (event_queue->tail + 1) % QUEUE_SIZE;
        pthread_cond_signal(&event_queue->cond);
    } else {
        LOG_ERROR("RingBuffer", "Event queue full, dropping event", __FILE__, __LINE__);
        pthread_mutex_unlock(&event_queue->mutex);
        return FAIL;
    }
    pthread_mutex_unlock(&event_queue->mutex);
    return SUCCESS; 
}

// Pop event from queue 
int event_queue_pop(EventQueue* event_queue, state_event_e* event, const char **requestId_out)
{
    pthread_mutex_lock(&event_queue->mutex);
    while (event_queue->head == event_queue->tail && !event_queue->shutdown) {
        pthread_cond_wait(&event_queue->cond, &event_queue->mutex);
    }
    if (event_queue->shutdown && event_queue->head == event_queue->tail) {
        pthread_mutex_unlock(&event_queue->mutex);
        *event = STATE_EVENT_shutdown; 
        return SUCCESS; 
    }

    *event = event_queue->events[event_queue->head];
    const char *popped_requestId = event_queue->requestIds[event_queue->head];
    if (requestId_out) {
        *requestId_out = popped_requestId; 
    } else {
        LOG_WARN("RingBuffer", "NULL requestId_out pointer provided, not returning requestId", __FILE__, __LINE__);
        if (popped_requestId) {
            free((char*)popped_requestId);
            event_queue->requestIds[event_queue->head] = NULL;
        }
        pthread_mutex_unlock(&event_queue->mutex);
        return FAIL; 
    }
    event_queue->head = (event_queue->head + 1) % QUEUE_SIZE;
    pthread_mutex_unlock(&event_queue->mutex);
    return SUCCESS;
}