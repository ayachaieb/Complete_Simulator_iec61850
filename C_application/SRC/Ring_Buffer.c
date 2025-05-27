#include "Ring_Buffer.h"
#include <stdio.h>
#include <pthread.h>
void event_queue_init(  EventQueue event_queue)
{
    pthread_mutex_init(&event_queue.mutex, NULL);
    pthread_cond_init(&event_queue.cond, NULL);
}

// Push event to queue
void event_queue_push(state_event_e event,  EventQueue event_queue)
{
    pthread_mutex_lock(&event_queue.mutex);
    if ((event_queue.tail + 1) % QUEUE_SIZE != event_queue.head) {
        event_queue.events[event_queue.tail] = event;
        event_queue.tail = (event_queue.tail + 1) % QUEUE_SIZE;
        printf("Pushed event: %d\n", event);
        pthread_cond_signal(&event_queue.cond);
    } else {
        fprintf(stderr, "Event queue full, dropping event\n");
    }
    pthread_mutex_unlock(&event_queue.mutex);
}
// Pop event from queue (blocks if empty)
state_event_e event_queue_pop(  EventQueue event_queue)
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
    printf("Popped event: %d\n", event);
    pthread_mutex_unlock(&event_queue.mutex);
    return event;
}
