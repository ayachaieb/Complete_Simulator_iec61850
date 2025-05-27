#include "../INC/Ring_Buffer.h"

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
        printf("Pushed event: %d\n", event);
        pthread_cond_signal(&event_queue.cond);
    } else {
        fprintf(stderr, "Event queue full, dropping event\n");
    }
    pthread_mutex_unlock(&event_queue.mutex);
}
