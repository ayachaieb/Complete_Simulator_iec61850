#include "Ring_Buffer.h"
#include <stdio.h>
#include <pthread.h>
void event_queue_init(EventQueue* event_queue)
{
    pthread_mutex_init(&event_queue->mutex, NULL);
    pthread_cond_init(&event_queue->cond, NULL);
}

// Push event to queue
void event_queue_push(state_event_e event, const char *requestId, EventQueue* event_queue)
{
    pthread_mutex_lock(&event_queue->mutex);
    if ((event_queue->tail + 1) % QUEUE_SIZE != event_queue->head) {
        event_queue->events[event_queue->tail] = event;

        if (requestId) {
            // *** THE FIX: Perform a deep copy using strdup ***
            event_queue->requestIds[event_queue->tail] = strdup(requestId);
            if (event_queue->requestIds[event_queue->tail] == NULL) {
                fprintf(stderr, "Error: strdup failed for requestId. Event might be pushed without requestId.\n");
                // Handle allocation failure: you might want to set to NULL or return an error
                event_queue->requestIds[event_queue->tail] = NULL;
            }
        } else {
            printf("Ring_buffer :: Warning: NULL requestId provided, storing NULL in queue\n");
            event_queue->requestIds[event_queue->tail] = NULL; // Handle NULL case explicitly
        }

        event_queue->tail = (event_queue->tail + 1) % QUEUE_SIZE;
        //printf("Pushed event: %d\n", event);
        // printf("RequestId pushed: %s (internal copy address: %p)\n",
        //        requestId ? requestId : "NULL",
        //        (void*)event_queue->requestIds[event_queue->tail]); // Optional: print internal address for debugging
        pthread_cond_signal(&event_queue->cond);
    } else {
        fprintf(stderr, "Event queue full, dropping event\n");
        // IMPORTANT: If you were strdup'ing here, you'd need to free the requestId if it's dropped.
        // But since it's only strdup'd if not full, this is okay.
    }
    pthread_mutex_unlock(&event_queue->mutex);
}
// Pop event from queue 
state_event_e event_queue_pop(EventQueue* event_queue,const char **requestId_out)
{
    pthread_mutex_lock(&event_queue->mutex);
    while (event_queue->head == event_queue->tail && !event_queue->shutdown) {
        pthread_cond_wait(&event_queue->cond, &event_queue->mutex);
    }
    if (event_queue->shutdown && event_queue->head == event_queue->tail) {
        pthread_mutex_unlock(&event_queue->mutex);
        return STATE_EVENT_shutdown;
    }

  state_event_e event = event_queue->events[event_queue->head];
    const char *popped_requestId = event_queue->requestIds[event_queue->head]; // Get the stored pointer
    if (requestId_out) {
        *requestId_out = popped_requestId; // Pass the pointer to the caller
    } else {
        printf("Ring_Buffer :: Warning: NULL requestId_out pointer provided, not returning requestId.\n");
        // If caller didn't provide a pointer, we still need to free the memory!
        if (popped_requestId) {
            free((char*)popped_requestId); // Free the memory we allocated with strdup
            event_queue->requestIds[event_queue->head] = NULL; // Clear the pointer in the queue
        }
    }
    event_queue->head = (event_queue->head + 1) % QUEUE_SIZE;
    //printf("Popped event: %d\n", event);
    pthread_mutex_unlock(&event_queue->mutex);
    return event;
}
