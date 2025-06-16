#include "Ring_Buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // Add this for strdup()
#include <pthread.h>
#include "util.h"
#include "logger.h"
#include <cjson/cJSON.h> 
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
int event_queue_push(state_event_e event, const char *requestId, EventQueue* event_queue, cJSON *data_obj) {
    int retval = SUCCESS;
    pthread_mutex_lock(&event_queue->mutex);
    
    if ((event_queue->tail + 1) % QUEUE_SIZE == event_queue->head) {
        LOG_ERROR("RingBuffer", "Event queue full, dropping event");
        retval = FAIL;
        goto unlock;
    }

    // Store event
    event_queue->events[event_queue->tail] = event;

    // Handle requestId
    if (requestId) {
        event_queue->requestIds[event_queue->tail] = strdup(requestId);
        if (!event_queue->requestIds[event_queue->tail]) {
            LOG_ERROR("RingBuffer", "strdup failed for requestId");
            retval = FAIL;
            goto unlock;
        }
    } else {
        event_queue->requestIds[event_queue->tail] = NULL;
    }

    // Handle JSON data
    if (data_obj) {
        event_queue->data_objs[event_queue->tail] = cJSON_Duplicate(data_obj, 1);
        if (!event_queue->data_objs[event_queue->tail]) {
            LOG_ERROR("RingBuffer", "Failed to duplicate JSON data");
            if (event_queue->requestIds[event_queue->tail]) {
                free((char*)event_queue->requestIds[event_queue->tail]);
                event_queue->requestIds[event_queue->tail] = NULL;
            }
            retval = FAIL;
            goto unlock;
        }
    } else {
        event_queue->data_objs[event_queue->tail] = NULL;
    }
    LOG_DEBUG("RingBuffer", "Pushed event: %s, requestId: %s",
              state_event_to_string(event), requestId ? requestId : "N/A");
    event_queue->tail = (event_queue->tail + 1) % QUEUE_SIZE;
    pthread_cond_signal(&event_queue->cond);

unlock:
    pthread_mutex_unlock(&event_queue->mutex);
    return retval;
}
// Pop event from queue 

int event_queue_pop(EventQueue* event_queue, state_event_e* event, const char **requestId_out, cJSON **data_obj_out) 
{
    int retval = SUCCESS;
    pthread_mutex_lock(&event_queue->mutex);
    
    while (event_queue->head == event_queue->tail && !event_queue->shutdown) {
        pthread_cond_wait(&event_queue->cond, &event_queue->mutex);
    }

    if (event_queue->shutdown && event_queue->head == event_queue->tail) {
        *event = STATE_EVENT_shutdown;
        retval = FAIL;
        goto unlock;
    }

    *event = event_queue->events[event_queue->head];
    
    // Transfer requestId
    if (requestId_out) {
        *requestId_out = event_queue->requestIds[event_queue->head];
    } else if (event_queue->requestIds[event_queue->head]) {
        free((char*)event_queue->requestIds[event_queue->head]);
    }
    event_queue->requestIds[event_queue->head] = NULL;
    
    // Transfer data_obj
    if (data_obj_out) {
        *data_obj_out = event_queue->data_objs[event_queue->head];
    } else if (event_queue->data_objs[event_queue->head]) {
        cJSON_Delete(event_queue->data_objs[event_queue->head]);
    }
    event_queue->data_objs[event_queue->head] = NULL;
    
    event_queue->head = (event_queue->head + 1) % QUEUE_SIZE;
    LOG_DEBUG("RingBuffer", "Popped event: %s, requestId: %s",
              state_event_to_string(*event), 
              requestId_out && *requestId_out ? *requestId_out : "N/A");

unlock:
    pthread_mutex_unlock(&event_queue->mutex);
    return retval;
}