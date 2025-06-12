#include "State_Machine.h"
#include <stdio.h>
#include "util.h"
#include "pthread.h"
#include "ipc.h"
#include "Ring_Buffer.h"
#include <cjson/cJSON.h>
#include <stdlib.h>
#include "logger.h"
#include "SV_Publisher.h"
#include <errno.h>
static state_machine_t sm_data_internal;
static EventQueue event_queue_internal;
static pthread_t sm_thread_internal;
#define FAIL -1
#define SUCCESS 0
// Function prototypes for state handlers
static bool state_idle_init(void *data);
static bool state_idle_enter(void *data, state_e from, state_event_e event, const char *requestId);

static bool state_init_init(void *data);
static bool state_init_enter(void *data, state_e from, state_event_e event, const char *requestId);

static bool state_running_init(void *data);
static bool state_running_enter(void *data, state_e from, state_event_e event, const char *requestId);

static bool state_stop_init(void *data);
static bool state_stop_enter(void *data, state_e from, state_event_e event, const char *requestId);
static void state_enter(state_machine_t *sm, state_e to, state_e from, state_event_e event, const char *requestId);
static void state_machine_free(state_machine_t *sm)
{
    free(sm->handlers);
    sm->handlers = NULL;
}

static int state_machine_init(state_machine_t *sm)
{
    int retval = SUCCESS;
    // Allocate handlers array for 4 states
    sm->handlers = calloc(4, sizeof(state_handler_t));
    if (!sm->handlers) {
        LOG_ERROR("State_Machine", "Failed to allocate handlers"); 
        retval= FAIL;
    }

else{
    sm->current_state = STATE_IDLE;
    // Initialize handlers
    sm->handlers[STATE_IDLE].init = state_idle_init;
    sm->handlers[STATE_IDLE].enter = state_idle_enter;
    sm->handlers[STATE_INIT].init = state_init_init;
    sm->handlers[STATE_INIT].enter = state_init_enter;
    sm->handlers[STATE_RUNNING].init = state_running_init;
    sm->handlers[STATE_RUNNING].enter = state_running_enter;
    sm->handlers[STATE_STOP].init = state_stop_init;
    sm->handlers[STATE_STOP].enter = state_stop_enter;
 

    // Call the enter function for the initial state
    if (sm->handlers[sm->current_state].enter) {
        sm->handlers[sm->current_state].enter( NULL,STATE_IDLE, STATE_EVENT_NONE,NULL);
    }
    else {
        LOG_ERROR("State_Machine", "Enter handler for initial state %d is NULL", sm->current_state);
        retval= FAIL;
    }
}
    return retval;
}


static int state_machine_run(state_machine_t *sm, state_event_e event, const char *requestId)
{
    int retval = SUCCESS;
    if (!sm) {
           LOG_ERROR("State_Machine", "State machine pointer is NULL in run function");
        retval = FAIL;
    }

    LOG_DEBUG("State_Machine", "State machine received event: %s, requestId: %s",
             state_event_to_string(event), requestId ? requestId : "N/A");

    // Handle shutdown event
    if (STATE_EVENT_shutdown ==event) {
        LOG_INFO("State_Machine", "State machine received shutdown event, transitioning to STOP state.");
        sm->current_state = STATE_STOP;
        state_enter(sm, sm->current_state, sm->current_state, event,requestId);
        retval = SUCCESS;
    }

    state_e current = sm->current_state;
    state_e next = current;

    // Transition logic
    switch (current) {
        case STATE_IDLE:
            if (STATE_EVENT_start_simulation == event ) {
                next = STATE_INIT;
            }
            else if ( STATE_EVENT_shutdown == event ) {
                next = STATE_STOP;
            }
            break;
        case STATE_INIT:
            if (STATE_EVENT_init_success == event ) {
                next = STATE_RUNNING;
            } else if (STATE_EVENT_init_failed == event ) {
                next = STATE_IDLE;
            } else if (STATE_EVENT_stop_simulation == event ) {
                next = STATE_STOP;
            }                                                                                                             
            break;
        case STATE_RUNNING:
            if (STATE_EVENT_stop_simulation == event  ) {
                next = STATE_STOP;
            }
            else if (STATE_EVENT_pause_simulation == event  ) {
                next = STATE_INIT;}
            break;
        case STATE_STOP:
        printf("State STOP\n");
        if (STATE_EVENT_start_simulation == event  ) {
                next = STATE_INIT;}
            break;
        default:
            break;
    }

    if (next != current || event != STATE_EVENT_NONE) {
        state_enter(sm, next, current, event,requestId);
    }
    return retval;
}

static void state_enter(state_machine_t *sm, state_e to, state_e from, state_event_e event, const char *requestId)
{
    if (from != to) {
        
    
    if (sm->handlers[to].enter) {
       // printf("StateMachine-state_enter::Calling enter handler for state %s\n", state_to_string(to));
        sm->handlers[to].enter(NULL, from, event,requestId);
    }
    sm->current_state = to;
}
}

static bool state_idle_init(void *data)
{
        return true;
    // Initialize idle state data here
}

static bool state_idle_enter( void *data,state_e from, state_event_e event, const char *requestId)
{
   // TRACE("Entered IDLE state from %s due to %s", state_to_string(from), state_event_to_string(event));
 LOG_INFO("State_Machine", "Entered IDLE state from %s due to %s", state_to_string(from), state_event_to_string(event));
 return true;
}
static bool state_init_init(void *data)
{
    // initialisation  sv initialisation 
     return true;
}

static bool state_init_enter(void *data ,state_e from, state_event_e event, const char *requestId)
{
    int retval = SUCCESS;
    LOG_INFO("State_Machine", "Entered INITIATION state from %s due to %s", state_to_string(from), state_event_to_string(event));
      
   const char* sv_interface = "lo"; 
    if (!SVPublisher_init(sv_interface)) {
        LOG_ERROR("State_Machine", "Failed to initialize SV Publisher module on interface %s", sv_interface);
        retval = FAIL;
    }
    else {
        LOG_INFO("State_Machine", "SV Publisher initialized successfully on interface %s", sv_interface);
        if(SUCCESS != StateMachine_push_event(STATE_EVENT_init_success, requestId)) {
            LOG_ERROR("State_Machine", "Failed to push init success event to state machine");
            retval = FAIL;
        }
        else {
            LOG_INFO("State_Machine", "Init success event pushed to state machine");
        }
    

    cJSON *json_response = cJSON_CreateObject();
    if (!json_response) {
         LOG_ERROR("State_Machine", "Failed to create JSON response object for event handler.");
        retval = FAIL;
    }
  const char *status_msg = "state init currently executing ...";
    cJSON_AddStringToObject(json_response, "status", status_msg);
    if (requestId) {
        cJSON_AddStringToObject(json_response, "requestId", requestId);
    }

    char *response_str = cJSON_PrintUnformatted(json_response);
    if (response_str) {
        if(ipc_send_response(response_str)== FAIL) {
              LOG_ERROR("State_Machine", "Failed to send response: %s", response_str);
        } else {
              LOG_INFO("State_Machine", "Response sent successfully: %s", response_str);
        }
        free(response_str); // Free the string allocated by cJSON_PrintUnformatted
    } else {
        LOG_ERROR("State_Machine", "Failed to serialize JSON response in event handler.");
    }

    cJSON_Delete(json_response); // Free the cJSON object
}
    return retval;

}

static bool state_running_init(void *data)
{
    // Initialize running state data here
    return true; 
}

static bool state_running_enter(void *data, state_e from, state_event_e event, const char *requestId)
{
    bool retval = SUCCESS;
 LOG_INFO("State_Machine", "Entered RUNNING state from %s due to %s", state_to_string(from), state_event_to_string(event));
  // Start the SV Publisher module here
    if (!SVPublisher_start()) {
        LOG_ERROR("State_Machine", "Failed to start SV Publisher module.");
       
        retval = FAIL;
    }
    LOG_INFO("State_Machine", "SV Publisher started in RUNNING state.");
   
 
 return retval;
}

static bool state_stop_init(void *data)
{
      return true;
}

static bool state_stop_enter(void *data , state_e from, state_event_e event, const char *requestId)
{
      LOG_INFO("State_Machine", "Entered STOP state from %s due to %s", state_to_string(from), state_event_to_string(event));
      
      // Stop the SV Publisher module here
    SVPublisher_stop();
    LOG_INFO("State_Machine", "SV Publisher stopped in STOP state.");

      cJSON *json_response = cJSON_CreateObject();
    if (!json_response) {
      LOG_ERROR("State_Machine", "Failed to create JSON response object for event handler.");
        return FAIL; 
    }
  const char *status_msg = "state STOP currently executing ...";
    cJSON_AddStringToObject(json_response, "status", status_msg);
    if (requestId) {
        cJSON_AddStringToObject(json_response, "requestId", requestId);
    }

    char *response_str = cJSON_PrintUnformatted(json_response);
    if (response_str) {
        if(ipc_send_response(response_str)== FAIL) {
          LOG_ERROR("State_Machine", "Failed to send response: %s", response_str);
        } else {
            LOG_INFO("State_Machine", "Response sent successfully: %s", response_str);
        }
        free(response_str); // Free the string allocated by cJSON_PrintUnformatted
    } else {
        LOG_ERROR("State_Machine", "Failed to serialize JSON response in event handler.");
    }

    cJSON_Delete(json_response); // Free the cJSON object
}


static void *state_machine_thread_internal(void *arg) {
    state_machine_t *sm = (state_machine_t *)arg;
    const char *requestId = NULL; // Initialize requestId to NULL
   
    // if (NULL == sm) {
    //       LOG_ERROR("State_Machine", "State machine pointer is NULL in internal thread");
    //     return NULL;
    // }
    int init_result =state_machine_init(sm); 
    if (init_result != SUCCESS) {
        LOG_ERROR("State_Machine", "State machine initialization failed in internal thread");
        return NULL;
    }
    while (1) {
        state_event_e event ;
        if (SUCCESS == event_queue_pop(&event_queue_internal,&event, &requestId)) {
            LOG_DEBUG("State_Machine", "State machine thread popped event: %s, requestId: %s",
                      state_event_to_string(event), requestId ? requestId : "N/A");
        } else {
            LOG_ERROR("State_Machine", "Failed to pop event from queue in internal thread");
        }

        if (STATE_EVENT_shutdown == event  ) {
            break;
        }
       if (SUCCESS!= state_machine_run(sm, event, requestId))
        {
            LOG_ERROR("State_Machine", "State machine run failed for event: %s, requestId: %s",
                      state_event_to_string(event), requestId ? requestId : "N/A");
        }
    }
    state_machine_free(sm);

    return NULL;
}

int StateMachine_Launch(void) {
  
    sm_data_internal.current_state = STATE_IDLE; 
    sm_data_internal.handlers = NULL; 
    // to be modified
     if (SUCCESS != event_queue_init(&event_queue_internal)){
     
        LOG_ERROR("State_Machine", "Failed to initialize event queue in module");
        return FAIL; 
    }


    if (pthread_create(&sm_thread_internal, NULL, state_machine_thread_internal, &sm_data_internal) != 0) {
          LOG_ERROR("State_Machine", "Failed to create state machine thread in module: %s", strerror(errno)); 
        return FAIL; // Indicate failure
    }
    LOG_INFO("State_Machine", "Created state machine thread in module");
    return EXIT_SUCCESS; 
}

int StateMachine_push_event(state_event_e event , const char *requestId) {

int result_event_queue_push = event_queue_push(event, requestId,&event_queue_internal); 
return result_event_queue_push;
}

int StateMachine_shutdown(void) {
    LOG_INFO("State_Machine", "Shutting down StateMachine module...");
    event_queue_internal.shutdown = EXIT_FAILURE; // Set shutdown flag to true
    int c1 = pthread_cond_signal(&event_queue_internal.cond); // Signal to wake up the thread
    int c2 = pthread_join(sm_thread_internal, NULL); // Wait for the thread to finish
    int c3 = pthread_mutex_destroy(&event_queue_internal.mutex); // Cleanup mutex
    int c4 = pthread_cond_destroy(&event_queue_internal.cond);   // Cleanup cond var
    LOG_INFO("State_Machine", "StateMachine module shutdown complete.");
    if (c1 != 0 || c2 != 0 || c3 != 0 || c4 != 0) {
        LOG_ERROR("State_Machine", "Error during StateMachine shutdown: cond_signal=%d, join=%d, mutex_destroy=%d, cond_destroy=%d",
                     c1, c2, c3,c4);
        return EXIT_FAILURE; // Indicate failure
    }

    return EXIT_SUCCESS; 
}