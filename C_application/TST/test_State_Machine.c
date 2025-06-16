int event_queue_push(state_event_e event, const char *requestId, EventQueue* event_queue, cJSON *data_obj)
{
    int retval = SUCCESS;

    // Lock the mutex at the very beginning of the critical section
    pthread_mutex_lock(&event_queue->mutex);

    // Check if the queue is full before attempting to add
    if ((event_queue->tail + 1) % QUEUE_SIZE== event_queue->head)
    {
        LOG_ERROR("RingBuffer", "Event queue full, dropping event");
        retval = FAIL;
        goto cleanup; // Go to cleanup to unlock mutex and return
    }

    // --- Process requestId ---
    if (NULL != requestId) {
        event_queue->requestIds[event_queue->tail] = strdup(requestId);
        if (event_queue->requestIds[event_queue->tail] == NULL) {
            LOG_ERROR("RingBuffer", "strdup failed for requestId (out of memory). Event cannot be pushed.");
            retval = FAIL;
            goto cleanup; // Go to cleanup to unlock mutex and return
        }
    } else {
        LOG_WARN("RingBuffer", "NULL requestId provided, storing NULL in queue");
        event_queue->requestIds[event_queue->tail] = NULL; // Store NULL as intended
    }

    // --- Process data_obj ---
    if (data_obj != NULL) {
        // Use cJSON_Duplicate to make a deep copy, so the original cJSON object can be freed by the sender
        event_queue->data_objs[event_queue->tail] = cJSON_Duplicate(data_obj, 1);
        if (event_queue->data_objs[event_queue->tail] == NULL) {
            LOG_ERROR("RingBuffer", "Failed to duplicate data_obj (out of memory). Event cannot be pushed.");
            // Clean up requestId if it was successfully duplicated before failing on data_obj
            if (event_queue->requestIds[event_queue->tail] != NULL) {
                free(event_queue->requestIds[event_queue->tail]);
                event_queue->requestIds[event_queue->tail] = NULL; // Clear pointer
            }
            retval = FAIL;
            goto cleanup; // Go to cleanup to unlock mutex and return
        }
    } else {
        LOG_DEBUG("RingBuffer", "NULL data_obj provided for event %d, storing NULL.", event);
        event_queue->data_objs[event_queue->tail] = NULL; // Store NULL as intended
    }

    // --- If all parts were successfully handled (no early `goto cleanup` occurred) ---
    // Place the event into the queue
    event_queue->events[event_queue->tail] = event;

    // Advance the tail pointer
    event_queue->tail = (event_queue->tail + 1) % QUEUE_SIZE;

    // Signal any waiting consumer threads
    pthread_cond_signal(&event_queue->cond);

cleanup:
    // Unlock the mutex right before returning, ensuring it's always unlocked
    pthread_mutex_unlock(&event_queue->mutex);
    return retval;
}