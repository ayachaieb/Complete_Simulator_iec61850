#include "util.h"
#include "hal_thread.h"
#include "linked_list.h"
#include "State_Machine.h"
#include "Goose_Listener.h"
#include "goose_receiver.h"
#include "goose_subscriber.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h> // For strerror
#include <signal.h>
#include "parser.h"
#include <pthread.h>
#include "logger.h"
#define  clockid_t CLOCK_REALTIME
volatile sig_atomic_t running_Goose = 1;
extern volatile bool internal_shutdown_flag;
bool goose_receiver_cleanup(void);
bool goose_receiver_is_running(void);
static pthread_t *threads = NULL;
static ThreadData *thread_data = NULL;
int goose_instance_count = 0;
// static void
// sigint_handler(int signalId)
// {
//     running_Goose = 0;
//     StateMachine_push_event(STATE_EVENT_shutdown, NULL, NULL);
//     internal_shutdown_flag = true; // Signal ipc_run_loop to exit
//     printf("Received SIGINT, shutting down...\n");
//     fflush(stdout);
//     goose_receiver_cleanup(); // Clean up GOOSE receiver resources
//     printf("GOOSE receiver  cleaned up.\n");
//     fflush(stdout);
// }
// static void sigint_handler(int signalId) {
//     running_Goose = 0;
//     internal_shutdown_flag = true;
//     StateMachine_push_event(STATE_EVENT_shutdown, NULL, NULL);
//     LOG_INFO("Goose_Listener", "Received SIGINT, pushing shutdown event...");
// }
static void gooseListener(GooseSubscriber subscriber, void *parameter)
{
    printf("\n--- GOOSE Event Received ---\n");
    fflush(stdout);
    LOG_INFO("Goose_Listener", "Message validity: %s",
             GooseSubscriber_isValid(subscriber) ? "valid" : "INVALID");

    if (!GooseSubscriber_isValid(subscriber))
    {
        LOG_ERROR("Goose_Listener", "Received invalid GOOSE message");
        return;
    }

    uint64_t timestamp = GooseSubscriber_getTimestamp(subscriber);
    LOG_INFO("Goose_Listener", "Timestamp: %llu ms", (long long unsigned int)timestamp);

    MmsValue *values = GooseSubscriber_getDataSetValues(subscriber);
    if (values == NULL)
    {
        LOG_ERROR("Goose_Listener", "Failed to get DataSet values");
        //printf("  AllData: NULL\n");
        fflush(stdout);
      //  printf("--------------------------\n");
        fflush(stdout);
        return;
    }

    char buffer[1024];
    if (MmsValue_printToBuffer(values, buffer, 1024))
    {
     //   printf("  AllData: %s\n", buffer);
    }
    else
    {
        LOG_ERROR("Goose_Listener", "Failed to print DataSet values (unknown type)");
      //  printf("  AllData: unknown type\n");
    }
   //printf("--------------------------\n");
    fflush(stdout);
}
// bool goose_receiver_cleanup(void)
// {
//     printf("STOPing GOOSE receiver and subscriber resources...\n");
//     fflush(stdout);
//     LOG_INFO("Goose_Listener", "Initiating GOOSE receiver cleanup");
//     running_Goose = false; // Stop the receiver loop
//     internal_shutdown_flag = true; // Ensure threads check both flags


//     // Wait for threads to terminate
//     if (threads != NULL)
//     {
//         for (int i = 0; i < goose_instance_count; ++i)
//         {
//             if (threads[i] != 0)
//             {
//                 LOG_INFO("Goose_Listener", "Joining thread %d", i);
//                 struct timespec timeout;
//                 clock_gettime(CLOCK_REALTIME, &timeout);
//                 timeout.tv_sec += 20; // Increase timeout to 10 seconds
//                 if (pthread_timedjoin_np(threads[i], NULL, &timeout) != 0)
//                 {
//                     LOG_ERROR("Goose_Listener", "Thread %d failed to join, detaching", i);
//                     pthread_detach(threads[i]);
//                 }
//                 else
//                 {
//                     printf("Goose_Listener Thread %d joined successfully", i);
//                     fflush(stdout);
//                 }
//                 threads[i] = 0; // Mark as joined/detached
//             }
//         }
//                     // Stop all receivers first
//     for (int i = 0; i < goose_instance_count; ++i)
//     {
//         if (thread_data[i].receiver != NULL)
//         {
//        //     printf("Goose_Listener", "Stopping GooseReceiver for instance %d", i);
//             GooseReceiver_stop(thread_data[i].receiver);
//             for (int retries = 0; retries < 5; retries++)
//             {
//                 if (!GooseReceiver_isRunning(thread_data[i].receiver))
//                 {
//                 //    printf("Goose_Listener", "GooseReceiver for instance %d stopped successfully", i);
//                     break;
//                 }
//                 LOG_WARN("Goose_Listener", "GooseReceiver for instance %d still running, retry %d", i, retries + 1);
//                 Thread_sleep(100);
//             }
//             if (GooseReceiver_isRunning(thread_data[i].receiver))
//             {
//              //   printf("Goose_Listener", "Failed to stop GooseReceiver for instance %d after retries", i);
//             }
//             GooseReceiver_destroy(thread_data[i].receiver);
//             thread_data[i].receiver = NULL;
//             thread_data[i].subscriber = NULL;
//         }
//     }

//        // printf("Goose_Listener", "Freeing threads array");
//         free(threads);
//         threads = NULL;
//     }

//     // Free thread_data resources
//     for (int j = 0; j < goose_instance_count; ++j)
//     {
//         LOG_INFO("Goose_Listener", "Freeing resources for thread_data[%d]", j);
//         if (thread_data[j].interface)
//         {
//             free(thread_data[j].interface);
//             thread_data[j].interface = NULL;
//         }
//         if (thread_data[j].GoCBRef)
//         {
//             free(thread_data[j].GoCBRef);
//             thread_data[j].GoCBRef = NULL;
//         }
//         if (thread_data[j].DatSet)
//         {
//             free(thread_data[j].DatSet);
//             thread_data[j].DatSet = NULL;
//         }
//         if (thread_data[j].MACAddress)
//         {
//             free(thread_data[j].MACAddress);
//             thread_data[j].MACAddress = NULL;
//         }
        
//     }

//     if (thread_data)
//     {
//         LOG_INFO("Goose_Listener", "Freeing thread_data");
//         free(thread_data);
//         thread_data = NULL;
//     }

//     //printf("GOOSE receiver and subscriber cleaned up.\n");
//     LOG_INFO("Goose_Listener", "GOOSE receiver cleanup completed");
//     return SUCCESS;
// }
bool goose_receiver_cleanup(void) {
    LOG_INFO("Goose_Listener", "Initiating GOOSE receiver cleanup");
    running_Goose = false; // Signal threads to exit
    internal_shutdown_flag = true;

    // Stop all receivers first
    for (int i = 0; i < goose_instance_count; ++i) {
        if (thread_data[i].receiver != NULL) {
            LOG_INFO("Goose_Listener", "Stopping GooseReceiver for instance %d", i);
            GooseReceiver_stop(thread_data[i].receiver);
            for (int retries = 0; retries < 10; retries++) {
                if (!GooseReceiver_isRunning(thread_data[i].receiver)) {
                    LOG_INFO("Goose_Listener", "GooseReceiver for instance %d stopped", i);
                    break;
                }
                LOG_WARN("Goose_Listener", "GooseReceiver for instance %d still running, retry %d", i, retries + 1);
                Thread_sleep(100);
            }
            if (GooseReceiver_isRunning(thread_data[i].receiver)) {
                LOG_ERROR("Goose_Listener", "Failed to stop GooseReceiver for instance %d after retries", i);
            }
            GooseReceiver_destroy(thread_data[i].receiver);
            thread_data[i].receiver = NULL;
            thread_data[i].subscriber = NULL;
        }
    }

    // Join threads
    if (threads != NULL) {
        for (int i = 0; i < goose_instance_count; ++i) {
            if (threads[i] != 0) {
                LOG_INFO("Goose_Listener", "Joining thread %d", i);
                struct timespec timeout;
                clock_gettime(CLOCK_REALTIME, &timeout);
                timeout.tv_sec += 2; // Reduced timeout
                if (pthread_timedjoin_np(threads[i], NULL, &timeout) != 0) {
                    LOG_ERROR("Goose_Listener", "Thread %d failed to join, detaching", i);
                    pthread_detach(threads[i]);
                } else {
                    LOG_INFO("Goose_Listener", "Thread %d joined successfully", i);
                }
                threads[i] = 0;
            }
        }
        free(threads);
        threads = NULL;
    }

    // Free thread_data resources
    for (int i = 0; i < goose_instance_count; ++i) {
        LOG_INFO("Goose_Listener", "Freeing resources for thread_data[%d]", i);
        free(thread_data[i].interface);
        free(thread_data[i].GoCBRef);
        free(thread_data[i].DatSet);
        free(thread_data[i].MACAddress);
        thread_data[i].interface = NULL;
        thread_data[i].GoCBRef = NULL;
        thread_data[i].DatSet = NULL;
        thread_data[i].MACAddress = NULL;
    }

    if (thread_data) {
        free(thread_data);
        thread_data = NULL;
    }

    goose_instance_count = 0;
    LOG_INFO("Goose_Listener", "GOOSE receiver cleanup completed");
    return SUCCESS;
}
// void *goose_thread_task(void *arg)
// {
//     ThreadData *data = (ThreadData *)arg;
//     LOG_INFO("Goose_Listener", "Thread started for appid 0x%d on interface %s", data->AppID, data->interface);
//     LOG_INFO("Goose_Listener", "Thread started for appid 0x%d on interface %s", data->AppID, data->interface);
//     LOG_INFO("Goose_Listener", "GoCBRef: %s, DatSet: %s, dstMac: %02x:%02x:%02x:%02x:%02x:%02x",
//              data->GoCBRef, data->DatSet,
//              data->MACAddress[0], data->MACAddress[1], data->MACAddress[2],
//              data->MACAddress[3], data->MACAddress[4], data->MACAddress[5]);
//     signal(SIGINT, sigint_handler);
//     if (data->receiver == NULL)
//     {
//         data->receiver = GooseReceiver_create();
//         if (data->receiver == NULL)
//         {
//             LOG_ERROR("Goose_Listener", "Failed to create GooseReceiver for appid 0x%d", data->AppID);
//             return NULL;
//         }
//     }

//     // Explicitly set the interface
//     GooseReceiver_setInterfaceId(data->receiver, data->interface);
//     LOG_INFO("Goose_Listener", "Set interface %s for appid 0x%d", data->interface, data->AppID);
//     data->subscriber = GooseSubscriber_create("simpleIOGenericIO/LLN0$GO$gcbAnalogValues",
//                                               "simpleIOGenericIO/LLN0$AnalogValues");
//     GooseSubscriber_setAppId(data->subscriber, 1000);
//     uint8_t mac[] = {0x01, 0x0c, 0xcd, 0x01, 0x00, 0x01};
//     GooseSubscriber_setDstMac(data->subscriber, mac);

//     //  data->subscriber = GooseSubscriber_create("IED1/LLN0$GO$gcbGoose1", "IED1/LLN0$DS$GooseDataSet1");
//     // data->subscriber = GooseSubscriber_create(data->GoCBRef, data->DatSet);
//     if (data->subscriber == NULL)
//     {
//         LOG_ERROR("Goose_Listener", "Failed to create GooseSubscriber for appid 0x%d", data->AppID);
//         GooseReceiver_destroy(data->receiver);
//         data->receiver = NULL;
//         return NULL;
//     }

//     // GooseSubscriber_setAppId(data->subscriber, data->AppID);

//     // GooseSubscriber_setDstMac(data->subscriber, data->MACAddress);
//     GooseSubscriber_setListener(data->subscriber, gooseListener, NULL);
//     GooseReceiver_addSubscriber(data->receiver, data->subscriber);

//     GooseReceiver_start(data->receiver);
//     if (!GooseReceiver_isRunning(data->receiver))
//     {
//         LOG_ERROR("Goose_Listener", "Failed to start GooseReceiver for appid 0x%d on interface %s",
//                   data->AppID, data->interface);
//         // printf("Failed to start GOOSE subscriber on interface %s for appid 0x%d.\n",
//         //        data->interface, data->AppID);
//         GooseReceiver_destroy(data->receiver);
//         data->receiver = NULL;
//         data->subscriber = NULL;
//         return NULL;
//     }

//     while ((running_Goose) && (!internal_shutdown_flag))
//     {
//         Thread_sleep(100);
//     }
//   //  printf("GOOSE thread exiting");
//     LOG_INFO("Goose_Listener", "GOOSE subscriber thread for appid 0x%d shutting down.", data->AppID);
//     // GooseReceiver_stop(data->receiver);
//     // GooseReceiver_destroy(data->receiver);
//     // data->receiver = NULL;
//     // data->subscriber = NULL;

//     return NULL;
// }
void *goose_thread_task(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    LOG_INFO("Goose_Listener", "Thread started for appid 0x%d on interface %s", data->AppID, data->interface);
    LOG_INFO("Goose_Listener", "GoCBRef: %s, DatSet: %s, dstMac: %02x:%02x:%02x:%02x:%02x:%02x",
             data->GoCBRef, data->DatSet,
             data->MACAddress[0], data->MACAddress[1], data->MACAddress[2],
             data->MACAddress[3], data->MACAddress[4], data->MACAddress[5]);

    data->receiver = GooseReceiver_create();
    if (data->receiver == NULL) {
        LOG_ERROR("Goose_Listener", "Failed to create GooseReceiver for appid 0x%d", data->AppID);
        return NULL;
    }

    GooseReceiver_setInterfaceId(data->receiver, data->interface);
    data->subscriber = GooseSubscriber_create(data->GoCBRef, data->DatSet);
    if (data->subscriber == NULL) {
        LOG_ERROR("Goose_Listener", "Failed to create GooseSubscriber for appid 0x%d", data->AppID);
        GooseReceiver_destroy(data->receiver);
        data->receiver = NULL;
        return NULL;
    }

    GooseSubscriber_setAppId(data->subscriber, data->AppID);
    GooseSubscriber_setDstMac(data->subscriber, data->MACAddress);
    GooseSubscriber_setListener(data->subscriber, gooseListener, NULL);
    GooseReceiver_addSubscriber(data->receiver, data->subscriber);

    GooseReceiver_start(data->receiver);
if (!GooseReceiver_isRunning(data->receiver)) {
        LOG_ERROR("Goose_Listener", "Failed to start GooseReceiver for appid 0x%d on interface %s",
                  data->AppID, data->interface);
        GooseReceiver_destroy(data->receiver);
        data->receiver = NULL;
        data->subscriber = NULL;
        return NULL;
    }

    while (running_Goose) {
        Thread_sleep(100);
    }

    LOG_INFO("Goose_Listener", "GOOSE subscriber thread for appid 0x%d shutting down", data->AppID);
    // Cleanup moved to goose_receiver_cleanup
    return NULL;
}

int Goose_receiver_init(SV_SimulationConfig *config, int number_of_subscribers)
{
    LOG_INFO("Goose_Listener", "Starting Goose_Listener");
    int retval = SUCCESS;
    if (config == NULL || number_of_subscribers <= 0)
    {
        LOG_ERROR("Goose_Listener", "Invalid input: instances array is NULL or number_publishers is non-positive.");
        return FAIL;
    }

    // Clean up any previous allocations if init is called multiple times without cleanup
    if (threads != NULL || thread_data != NULL)
    {
        LOG_INFO("Goose_Listener", "Previous SV Publisher instances found. Cleaning up before re-initialization.");

        if (threads)
        {
            free(threads);
            threads = NULL;
        }
        if (thread_data)
        {
            // Need to free internal strings within thread_data if they were strdup'd
            for (int i = 0; i < number_of_subscribers; ++i)
            {

                if (thread_data[i].interface)
                    free(thread_data[i].interface);
        
                if (thread_data[i].GoCBRef)
                    free(thread_data[i].GoCBRef);
                if (thread_data[i].DatSet)
                    free(thread_data[i].DatSet);
                if (thread_data[i].MACAddress)
                    free(thread_data[i].MACAddress);
            }
            free(thread_data);
            thread_data = NULL;
        }
        goose_instance_count = 0;
    }

    goose_instance_count = number_of_subscribers;

    threads = (pthread_t *)malloc(goose_instance_count * sizeof(pthread_t));
    if (!threads)
    {
        LOG_ERROR("Goose_Listener", "Memory allocation failed for threads!");
        return FAIL;
    }
    memset(threads, 0, goose_instance_count * sizeof(pthread_t)); // Initialize to 0

    thread_data = (ThreadData *)malloc(goose_instance_count * sizeof(ThreadData));
    if (!thread_data)
    {
        LOG_ERROR("Goose_Listener", "Memory allocation failed for thread_data!");
        free(threads); // Clean up threads array
        threads = NULL;
        return FAIL;
    }
    memset(thread_data, 0, goose_instance_count * sizeof(ThreadData)); // Initialize to 0
    int i = 0;
    // LOG_INFO("Goose_Listener", "Initializing %d SV Publisher instances", goose_instance_count);
    for (i = 0; i < goose_instance_count; i++)
    {
        // Initialize thread_data[i] to ensure all pointers are NULL before strdup
        memset(&thread_data[i], 0, sizeof(ThreadData));
        thread_data[i].receiver = NULL;   // or create if necessary
        thread_data[i].subscriber = NULL; // or create if necessary
        if (config[i].appId)
        {
            char *endptr;
            unsigned long val = strtoul(config[i].appId, &endptr, 10); // Base 10 for numeric appId
            if (*endptr != '\0' || val > UINT32_MAX)
            {
                LOG_ERROR("Goose_Listener", "Invalid appId format or value for instance %d: %s", i, config[i].appId);
                goto cleanup_init_failure;
            }

            // If you need to convert appId to an integer, do it here:
            //  thread_data[i].GOOSEappId = 0x5000 + atoi(instances[i].appId);
            // For now, GOOSEappId is also a string from JSON, so we'll assume it's handled elsewhere or convert it.
            // Let's assume GOOSEappId is derived from appId string, so it should be uint32_t
            thread_data[i].AppID = (uint32_t)val;                                   // Store the numeric value directly
            thread_data[i].goose_id = (uint32_t)strtoul(config[i].appId, NULL, 10); // Convert string appId to uint32_t
        }
        else
        {
            LOG_ERROR("Goose_Listener", "appId is NULL for instance %d", i);
            goto cleanup_init_failure;
        }

        if (config[i].Interface)
        {
            thread_data[i].interface = strdup(config[i].Interface); // Example: fixed string

            if (!thread_data[i].interface)
            {
                LOG_ERROR("Goose_Listener", "Memory allocation failed for interface for instance %d", i);
                goto cleanup_init_failure;
            }
        }
        else
        {
            LOG_ERROR("Goose_Listener", "interface is NULL for instance %d", i);
            goto cleanup_init_failure;
        }
     //   printf("gooose interface: %s\n", thread_data[i].interface);

        if (config[i].GoCBRef)
        {
            thread_data[i].GoCBRef = strdup(config[i].GoCBRef); // Example: fixed string

            if (!thread_data[i].GoCBRef)
            {
                LOG_ERROR("Goose_Listener", "Memory allocation failed for goCbRef for instance %d", i);
                goto cleanup_init_failure;
            }
        }
        else
        {
            LOG_ERROR("Goose_Listener", "GoCBRef is NULL for instance %d", i);
            goto cleanup_init_failure;
        }
       // printf("GoCBRef: %s\n", thread_data[i].GoCBRef);

        if (config[i].DatSet)
        {
            thread_data[i].DatSet = strdup(config[i].DatSet);
            if (!thread_data[i].DatSet)
            {
                LOG_ERROR("Goose_Listener", "Memory allocation failed for DatSet for instance %d", i);
                goto cleanup_init_failure;
            }
        }
        else
        {
            LOG_ERROR("Goose_Listener", "DatSet is NULL for instance %d", i);
            goto cleanup_init_failure;
        }
        LOG_INFO("Goose_Listener", "DatSet: %s", thread_data[i].DatSet);

        // Parse and copy MAC address
        // Parse and copy MAC address
        thread_data[i].MACAddress = (uint8_t *)malloc(6 * sizeof(uint8_t));
        if (NULL != thread_data[i].MACAddress)
        {
            if (!parse_mac_address(config[i].MACAddress, thread_data[i].MACAddress))
            {
                LOG_ERROR("Goose_Listener", "Failed to parse MAC address for instance %d", i);
                goto cleanup_init_failure;
            }
        }
        else
        {
            LOG_ERROR("Goose_Listener", "dstMac is NULL for instance %d", i);
            goto cleanup_init_failure;
        }
        LOG_INFO("Goose_Listener", "dstMac: %02x:%02x:%02x:%02x:%02x:%02x",
                 thread_data[i].MACAddress[0], thread_data[i].MACAddress[1],
                 thread_data[i].MACAddress[2], thread_data[i].MACAddress[3],
                 thread_data[i].MACAddress[4], thread_data[i].MACAddress[5]);

        LOG_INFO("Goose_Listener", "All thread_data initialized successfully");

        //  thread_data[i].subscriber = GooseSubscriber_create("", NULL);
        // thread_data[i].subscriber = GooseSubscriber_create(thread_data[i].GoCBRef, NULL);
        // GooseSubscriber_setDstMac(thread_data[i].subscriber, thread_data[i].MACAddress);

        //  GooseSubscriber_setAppId(thread_data[i].subscriber, thread_data[i].AppID);
        // for Goose_receiver_start
        //  GooseSubscriber_setListener(subscriber, gooseListener, NULL);
        //  GooseReceiver_addSubscriber(receiver, subscriber);
    }
    return SUCCESS;

cleanup_init_failure:
    LOG_INFO("Goose_Listener", "cleanup happening");
    // Free all memory allocated so far for thread_data and threads
    for (int j = 0; j <= i; ++j)
    { // Free up to the current failed instance

        if (thread_data[j].interface)
            free(thread_data[j].interface);
        if (thread_data[j].GoCBRef)
            free(thread_data[j].GoCBRef);
        if (thread_data[j].DatSet)
            free(thread_data[j].DatSet);
        if (thread_data[j].MACAddress)
            free(thread_data[j].MACAddress);
    }
    if (thread_data)
    {
        free(thread_data);
        thread_data = NULL; // Set to NULL to avoid dangling pointer
    }
    if (threads)
    {
        free(threads);
        threads = NULL;
    }
    goose_instance_count = 0;
    return FAIL;
}

int Goose_receiver_start()
{
    if (threads == NULL || thread_data == NULL || goose_instance_count <= 0)
    {
        LOG_ERROR("Goose_Listener", "SV Publisher not initialized. Call SVPublisher_init first.");
        return FAIL;
    }
    //signal(SIGINT, sigint_handler);
    bool all_threads_created = SUCCESS;
    for (int i = 0; i < goose_instance_count; i++)
    {
        if (pthread_create(&threads[i], NULL, goose_thread_task, &thread_data[i]) != 0)
        {
            LOG_ERROR("Goose_Listener", "Failed to create thread for instance %d: %s", i, strerror(errno));
            all_threads_created = FAIL;
        }
        else
        {
         //   printf("Created a goooooose   thread for instance %d\n", i);
            LOG_INFO("Goose_Listener", "Created thread for instance %d", i);
        }
    }
    // printf("All threads created successfully: %d\n", all_threads_created);
    LOG_INFO("Goose_Listener", "Goose_Listenerthreads started.");

    return all_threads_created;
}