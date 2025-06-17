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
#include "parser.h" 
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
static bool state_init_enter(void *data, state_e from, state_event_e event, const char *requestId, cJSON *data_obj);

static bool state_running_init(void *data);
static bool state_running_enter(void *data, state_e from, state_event_e event, const char *requestId, cJSON *data_obj);

static bool state_stop_init(void *data);
static bool state_stop_enter(void *data, state_e from, state_event_e event, const char *requestId);

static void state_enter(state_machine_t *sm, state_e to, state_e from, state_event_e event, const char *requestId, cJSON *data_obj); 

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
        sm->handlers[sm->current_state].enter( NULL,STATE_IDLE, STATE_EVENT_NONE,NULL,NULL);
    }
    else {
        LOG_ERROR("State_Machine", "Enter handler for initial state %d is NULL", sm->current_state);
        retval= FAIL;
    }
}
    return retval;
}


static int state_machine_run(state_machine_t *sm, state_event_e event, const char *requestId, cJSON *data_obj)
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
        state_enter(sm, sm->current_state, sm->current_state, event,requestId, data_obj);
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
                if (data_obj != NULL) {
                next = STATE_RUNNING;
            } else {
                LOG_ERROR("State_Machine", "init_success event missing required configuration data");
                next = STATE_IDLE;}
            } else if (STATE_EVENT_init_failed == event ) {
                next = STATE_IDLE;
                StateMachine_push_event(STATE_EVENT_init_failed, requestId, NULL);
            } else if (STATE_EVENT_stop_simulation == event || 
                       STATE_EVENT_shutdown == event)  {
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
        state_enter(sm, next, current, event,requestId, data_obj);
    }
    return retval;
}

static void state_enter(state_machine_t *sm, state_e to, state_e from, state_event_e event, const char *requestId, cJSON *data_obj)
{
    if (from != to) {
        
    
    if (sm->handlers[to].enter) {
       // printf("StateMachine-state_enter::Calling enter handler for state %s\n", state_to_string(to));
     

     
        sm->handlers[to].enter(NULL, from, event,requestId, data_obj);
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
static bool state_init_enter(void *data ,state_e from, state_event_e event, const char *requestId, cJSON *data_obj)
{
    int retval = SUCCESS;
    LOG_INFO("State_Machine", "Entered INITIATION state from %s due to %s", state_to_string(from), state_event_to_string(event));
      
    // Declare a dynamic array or linked list to store parsed configurations if needed globally
    // For this example, we will process them within the loop.

    // data_obj now holds the cJSON array of configurations
    if (cJSON_IsArray(data_obj)) {
        int array_size = cJSON_GetArraySize(data_obj);
        LOG_INFO("State_Machine", "Received %d configuration instances.", array_size);

        // Example: Store configurations in a dynamically allocated array
        // SV_SimulationConfig *all_configs = malloc(array_size * sizeof(SV_SimulationConfig));
        // if (!all_configs) {
        //     LOG_ERROR("State_Machine", "Failed to allocate memory for all configurations.");
        //     return FAIL;
        // }

        // Variable to hold the interface for SVPublisher_init (e.g., from the first config)
        const char* sv_interface_for_publisher = NULL;
        SV_SimulationConfig svconfig_tab[array_size]; // Array to hold all configurations if needed
        for (int i = 0; i < array_size; i++) {
            cJSON *instance_json_obj = cJSON_GetArrayItem(data_obj, i);
            if (instance_json_obj) {
                SV_SimulationConfig current_config = {0}; // Initialize a new config struct for each instance
                if (parseSVconfig(instance_json_obj, &current_config) == SUCCESS) {
                    svconfig_tab[i] = current_config; // Store the parsed config in the array
                    LOG_INFO("State_Machine", "Successfully parsed instance %d: appId=%s, dstMac=%s, svInterface=%s, scenarioConfigFile=%s, svIDs=%s", 
                             i, current_config.appId, current_config.dstMac, current_config.svInterface, current_config.scenarioConfigFile, current_config.svIDs);
                    
                    // Example: Use the svInterface from the first config for SVPublisher_init
                    if (i == 0) {
                         sv_interface_for_publisher = strdup(current_config.svInterface); 
                    }

                    // If you need to store all configurations, uncomment and use all_configs[i] = current_config;
                    // Be careful with memory management if storing: you might need to deep copy strings.

                    // IMPORTANT: Free the dynamically allocated strings within current_config
                    // if you are not storing them or if you are done processing this single config.
                    // If you store them in an array, free them when the array is no longer needed.
                    // For this example, we free them immediately after processing.
                    free(current_config.appId);
                    free(current_config.dstMac);
                    free(current_config.svInterface);
                    free(current_config.scenarioConfigFile);
                    free(current_config.svIDs);

                } else {
                    LOG_ERROR("State_Machine", "Failed to parse instance %d.", i);
                    retval = FAIL; // Mark as failed if any instance fails to parse
                }
            } else {
                LOG_ERROR("State_Machine", "Failed to get instance %d from array.", i);
                retval = FAIL; // Mark as failed if any instance is missing
            }
        }

        // Now, after parsing all configurations, proceed with SVPublisher_init or other actions
        if (retval == SUCCESS) { // Only proceed if all parsing was successful
            if (sv_interface_for_publisher) {
                if (!SVPublisher_init(sv_interface_for_publisher)) {
                    LOG_ERROR("State_Machine", "Failed to initialize SV Publisher module on interface %s", sv_interface_for_publisher);
                    retval = FAIL;
                } else {
                    LOG_INFO("State_Machine", "SV Publisher initialized successfully on interface %s", sv_interface_for_publisher);
                }
            } else {
                LOG_ERROR("State_Machine", "No SV interface found for SV Publisher initialization.");
                retval = FAIL;
            }
        }

    } else {
        LOG_ERROR("State_Machine", "Expected \'data\' to be a JSON array, but it\'s not.");
        retval = FAIL;
    }

    // Handle success/failure and push events
    if (retval == SUCCESS) {
        if(SUCCESS != StateMachine_push_event(STATE_EVENT_init_success, requestId, data_obj)) {
            LOG_ERROR("State_Machine", "Failed to push init success event to state machine");
            retval = FAIL;
        } else {
            LOG_INFO("State_Machine", "Init success event pushed to state machine");
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
    } else {
        // If initialization failed, push init_failed event
        if(SUCCESS != StateMachine_push_event(STATE_EVENT_init_failed, requestId, data_obj)) {
            LOG_ERROR("State_Machine", "Failed to push init failed event to state machine");
        }
    }

    // No cleanup for config.appID etc. here, as they are freed within the loop for each current_config
    // If you store all_configs, you would free them after the loop or when all_configs is no longer needed.
    
    return retval;
}



static bool state_running_init(void *data)
{
    // Initialize running state data here
    return true; 
}

static bool state_running_enter(void *data, state_e from, state_event_e event, 
                              const char *requestId, cJSON *data_obj) {
   // SV_SimulationConfig config;  // Use stack allocation instead of pointer
    bool retval = SUCCESS;
    
    LOG_INFO("State_Machine", "Entered RUNNING state from %s due to %s", 
            state_to_string(from), state_event_to_string(event));

    // Initialize config (critical!)
   // memset(&config, 0, sizeof(SV_SimulationConfig));

    // Parse configuration
    // if (parseSVconfig(data_obj, &config) != SUCCESS) {
    //     LOG_ERROR("State_Machine", "Failed to parse SV configuration");
    //     retval = FAIL;
    //     goto cleanup;
    // }

    // Validate parsed fields
    // if (!config.appID || !config.macAddress || !config.interface || 
    //     !config.svid || !config.scenariofile) {
    //     LOG_ERROR("State_Machine", "Invalid SV config (NULL field detected)");
    //     retval = FAIL;
    //     goto cleanup;
    // }

    // LOG_INFO("State_Machine", 
    //        "SV config parsed: appID=%s, mac=%s, interface=%s, svid=%s, scenario=%s",
    //        config.appID, config.macAddress, config.interface, 
    //        config.svid, config.scenariofile);

    // Start publisher
    if (!SVPublisher_start()) {
        LOG_ERROR("State_Machine", "Failed to start SV Publisher");
        retval = FAIL;
      //  goto cleanup;
    }

// cleanup:
//     // Free allocated resources
//     free(config.appID);
//     free(config.macAddress);
//     free(config.interface);
//     free(config.svid);
//     free(config.scenariofile);
    
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
    const char *requestId = NULL;
    cJSON *data_obj = NULL;  // Change from cJSON** to cJSON*
    
    if (NULL == sm) {
        LOG_ERROR("State_Machine", "State machine pointer is NULL in internal thread");
        return NULL;
    }

    int init_result = state_machine_init(sm); 
    if (init_result != SUCCESS) {
        LOG_ERROR("State_Machine", "State machine initialization failed in internal thread");
        return NULL;
    }

    while (1) {
        state_event_e event;
        if (SUCCESS == event_queue_pop(&event_queue_internal, &event, &requestId, &data_obj)) {
            LOG_DEBUG("State_Machine", "popped event: %s, requestId: %s",
                     state_event_to_string(event), requestId ? requestId : "N/A");
        } else {
            LOG_ERROR("State_Machine", "Failed to pop event from queue in internal thread");
        }

        if (STATE_EVENT_shutdown == event) {
            if (data_obj) cJSON_Delete(data_obj);
            break;
        }

        // Now we can use data_obj directly since it's cJSON*
        if (SUCCESS != state_machine_run(sm, event, requestId, data_obj)) {
            LOG_ERROR("State_Machine", "State machine run failed for event: %s, requestId: %s",
                     state_event_to_string(event), requestId ? requestId : "N/A");
        }

        // Clean up after processing
        if (requestId) {
            free((char*)requestId);
            requestId = NULL;
        }
        if (data_obj) {
            cJSON_Delete(data_obj);
            data_obj = NULL;
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

int StateMachine_push_event(state_event_e event , const char *requestId, cJSON *data_obj)
{
    int result_event_queue_push = SUCCESS;
    if (event == STATE_EVENT_NONE) {
        LOG_ERROR("State_Machine", "Attempted to push an event with STATE_EVENT_NONE");
        return EXIT_FAILURE; // Invalid event
    }

    if (event_queue_internal.shutdown) {
        LOG_ERROR("State_Machine", "Event queue is shutting down, cannot push event: %s", state_event_to_string(event));
        return EXIT_FAILURE; // Queue is shutting down
    }

    if (NULL!=data_obj) {

        int result_event_queue_push = event_queue_push(event, requestId,&event_queue_internal, data_obj);   

          LOG_INFO("State_Machine", "Pushing event: %s, requestId: %s",
                  state_event_to_string(event), requestId ? requestId : "N/A");
    } else {
        result_event_queue_push = event_queue_push(event, requestId,&event_queue_internal, data_obj);  
        LOG_INFO("State_Machine"," Pushing event without data: %s, requestId: %s",
                  state_event_to_string(event), requestId ? requestId : "N/A");
                 

    }

    return result_event_queue_push;
}

int StateMachine_shutdown(void) 
{
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