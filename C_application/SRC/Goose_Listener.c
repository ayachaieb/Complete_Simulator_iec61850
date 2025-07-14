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
#include <sys/time.h>
volatile sig_atomic_t running_Goose = 1;
extern volatile bool internal_shutdown_flag;
static volatile sig_atomic_t cleanup_in_progress = 0;
bool goose_receiver_cleanup(void);
bool goose_receiver_is_running(void);
static pthread_t *threads = NULL;
static ThreadData *thread_data = NULL;
int goose_instance_count = 0;
static pthread_mutex_t goose_cleanup_mutex = PTHREAD_MUTEX_INITIALIZER;
static void goose_thread_cleanup(void *arg);

void sigint_handler_Goose(int signalId)
{
    // Only set atomic flags in signal handler - this is async-signal-safe
    running_Goose = 0;
    internal_shutdown_flag = 1;
    cleanup_in_progress = 1;
}

void monitor_shutdown() {
    while (running_Goose) {
        sleep(1);
    }
    
    // Do cleanup here, not in signal handler
    printf("Shutdown requested, cleaning up...\n");
    fflush(stdout);
    
    if (SUCCESS == goose_receiver_cleanup()) {
        printf("GOOSE receiver cleanup completed successfully.\n");
    } else {
        printf("GOOSE receiver cleanup failed.\n");
    }
    
    exit(0);
}
long long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
static void
gooseListener(GooseSubscriber subscriber, void *parameter)
{
    static uint64_t last_print_time = 0;
uint64_t now = get_current_time_ms(); // Implement this function
if (now - last_print_time > 1000) { // Only print once per second
    last_print_time = now;
    LOG_INFO("Goose_Listener", "GOOSE message received");
    printf("GOOSE message received\n");
}
    if (GooseSubscriber_isVlanSet(subscriber))
    {
        //  printf("    vlanId: %u\n", GooseSubscriber_getVlanId(subscriber));
        // printf("    vlanPrio: %u\n", GooseSubscriber_getVlanPrio(subscriber));
    }
    //  printf("  appId: %d\n", GooseSubscriber_getAppId(subscriber));
    uint8_t macBuf[6];
    GooseSubscriber_getSrcMac(subscriber, macBuf);
    //  printf("  srcMac: %02X:%02X:%02X:%02X:%02X:%02X\n", macBuf[0],macBuf[1],macBuf[2],macBuf[3],macBuf[4],macBuf[5]);
    GooseSubscriber_getDstMac(subscriber, macBuf);

    //  printf("  dstMac: %02X:%02X:%02X:%02X:%02X:%02X\n", macBuf[0],macBuf[1],macBuf[2],macBuf[3],macBuf[4],macBuf[5]);
    // printf("  goId: %s\n", GooseSubscriber_getGoId(subscriber));
    // printf("  goCbRef: %s\n", GooseSubscriber_getGoCbRef(subscriber));
    // printf("  dataSet: %s\n", GooseSubscriber_getDataSet(subscriber));
    //  printf("  confRev: %u\n", GooseSubscriber_getConfRev(subscriber));
    //  printf("  ndsCom: %s\n", GooseSubscriber_needsCommission(subscriber) ? "true" : "false");
    // printf("  simul: %s\n", GooseSubscriber_isTest(subscriber) ? "true" : "false");
    // printf("  stNum: %u sqNum: %u\n", GooseSubscriber_getStNum(subscriber),
    //     GooseSubscriber_getSqNum(subscriber));

    uint64_t timestamp = GooseSubscriber_getTimestamp(subscriber);
    MmsValue *values = GooseSubscriber_getDataSetValues(subscriber);

    char buffer[1024];

    MmsValue_printToBuffer(values, buffer, 1024);

    //printf("  AllData: %s\n", buffer);
    //  printf("--------------------------\n");
    //fflush(stdout);
}

static void goose_thread_cleanup(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    if (data == NULL) return;

    LOG_INFO("Goose_Listener", "Cleaning up thread resources");
    
    if (data->receiver != NULL) {
        GooseReceiver_stop(data->receiver);
        GooseReceiver_destroy(data->receiver);
        data->receiver = NULL;
    }

}
void *goose_thread_task(void *arg) 
{
    ThreadData *data = (ThreadData *)arg;
    int ret = SUCCESS;
    
    if (data == NULL) {
        return (void*)(intptr_t)FAIL;
    }
    
    // Set cancellation points
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    
    // Block signals that might interfere with cleanup
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGRTMIN);
    sigaddset(&mask, SIGRTMIN+1);
    sigaddset(&mask, SIGRTMIN+2);
    sigaddset(&mask, SIGRTMIN+3);
    sigaddset(&mask, SIGRTMIN+4);
    sigaddset(&mask, SIGRTMIN+5);
    pthread_sigmask(SIG_BLOCK, &mask, &oldmask);
    
    // Install cleanup handler
    pthread_cleanup_push(goose_thread_cleanup, data);
    
    LOG_INFO("Goose_Listener", "Thread started for appid 0x%04x", data->AppID);
    
    // Initialize receiver
    data->receiver = GooseReceiver_create();
    if (!data->receiver) {
        LOG_ERROR("Goose_Listener", "Receiver creation failed");
        ret = FAIL;
        goto cleanup;
    }
    
    GooseReceiver_setInterfaceId(data->receiver, data->interface);
    
    // Use the actual GoCBRef from thread_data instead of hardcoded string
    data->subscriber = GooseSubscriber_create(data->GoCBRef, NULL);
    if (!data->subscriber) {
        LOG_ERROR("Goose_Listener", "Subscriber creation failed");
        ret = FAIL;
        goto cleanup;
    }
    
    // Use the actual AppID from thread_data instead of hardcoded 1000
    GooseSubscriber_setAppId(data->subscriber, data->AppID);
    
    // Use the actual MAC address from thread_data instead of hardcoded MAC
    GooseSubscriber_setDstMac(data->subscriber, data->MACAddress);
    
    GooseSubscriber_setListener(data->subscriber, gooseListener, NULL);
    GooseReceiver_addSubscriber(data->receiver, data->subscriber);
    
    GooseReceiver_start(data->receiver);
    
    // Check if receiver started successfully
    if (!GooseReceiver_isRunning(data->receiver)) {
        LOG_ERROR("Goose_Listener", "Failed to start receiver");
        ret = FAIL;
        goto cleanup;
    }
    
    // Main loop with proper cancellation handling
    while (running_Goose && !internal_shutdown_flag) {
        pthread_testcancel(); // Cancellation point
        
        if (!GooseReceiver_isRunning(data->receiver)) {
            LOG_WARN("Goose_Listener", "Receiver stopped unexpectedly");
            ret = FAIL;
            break;
        }
        
        // Sleep with cancellation point - use nanosleep instead of Thread_sleep
        struct timespec ts = {0, 50000000}; // 50ms
        if (nanosleep(&ts, NULL) == -1 && errno == EINTR) {
            // Handle interruption gracefully
            pthread_testcancel();
        }
    }
    
cleanup:
    // Restore signal mask before cleanup
    pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
    
    // Pop and execute cleanup handler
    pthread_cleanup_pop(1);
    
    LOG_INFO("Goose_Listener", "Thread exiting for appid 0x%04x", data->AppID);
    return (ret == SUCCESS) ? NULL : (void*)(intptr_t)ret;
}
bool goose_receiver_cleanup(void) {
    pthread_mutex_lock(&goose_cleanup_mutex);
    
    // Step 1: Signal all threads to stop gracefully
    running_Goose = false;
    internal_shutdown_flag = true;
    
    // Step 2: Stop all receivers first
    for (int i = 0; i < goose_instance_count; ++i) {
        if (thread_data[i].receiver && GooseReceiver_isRunning(thread_data[i].receiver)) {
            GooseReceiver_stop(thread_data[i].receiver);
        }
    }
    
    // Give threads time to exit gracefully
    struct timespec sleep_time = {0, 100000000}; // 100ms
    nanosleep(&sleep_time, NULL);
    
    // Step 3: Join threads with timeout
    if (threads != NULL) {
        for (int i = 0; i < goose_instance_count; ++i) {
            if (threads[i] != 0) {
                void *thread_result;
                struct timespec timeout;
                clock_gettime(CLOCK_REALTIME, &timeout);
                timeout.tv_sec += 3; // 3 second timeout
                
                int rc = pthread_timedjoin_np(threads[i], &thread_result, &timeout);
                if (rc == ETIMEDOUT) {
                    LOG_ERROR("Goose_Listener", "Thread %d timeout - canceling", i);
                    pthread_cancel(threads[i]);
                    
                    // Try to join after cancel
                    timeout.tv_sec += 1;
                    rc = pthread_timedjoin_np(threads[i], NULL, &timeout);
                    if (rc == ETIMEDOUT) {
                        LOG_ERROR("Goose_Listener", "Thread %d still hanging - detaching", i);
                        pthread_detach(threads[i]);
                    }
                } else if (rc != 0) {
                    LOG_ERROR("Goose_Listener", "Thread %d join error: %s", i, strerror(rc));
                }
                threads[i] = 0;  // Mark as handled
            }
        }
        free(threads);
        threads = NULL;
    }
    
    // Step 4: Clean up all resources
    for (int i = 0; i < goose_instance_count; ++i) {
        // Clean up GooseReceiver and GooseSubscriber
        if (thread_data[i].receiver != NULL) {
            if (GooseReceiver_isRunning(thread_data[i].receiver)) {
                GooseReceiver_stop(thread_data[i].receiver);
            }
            GooseReceiver_destroy(thread_data[i].receiver);
            thread_data[i].receiver = NULL;
        }
        
        if (thread_data[i].subscriber != NULL) {
            GooseSubscriber_destroy(thread_data[i].subscriber);
            thread_data[i].subscriber = NULL;
        }
        
        // Free allocated strings with null checks
        if (thread_data[i].interface) {
            free(thread_data[i].interface);
            thread_data[i].interface = NULL;
        }
        if (thread_data[i].GoCBRef) {
            free(thread_data[i].GoCBRef);
            thread_data[i].GoCBRef = NULL;
        }
        if (thread_data[i].DatSet) {
            free(thread_data[i].DatSet);
            thread_data[i].DatSet = NULL;
        }
        if (thread_data[i].MACAddress) {
            free(thread_data[i].MACAddress);
            thread_data[i].MACAddress = NULL;
        }
        
        memset(&thread_data[i], 0, sizeof(ThreadData));
    }
    
    if (thread_data) {
        free(thread_data);
        thread_data = NULL;
    }
    goose_instance_count = 0;
    
    pthread_mutex_unlock(&goose_cleanup_mutex);
    LOG_INFO("Goose_Listener", "GOOSE cleanup complete");
    return SUCCESS;
}

// // If pthread_timedjoin_np is not available on your system, use this alternative:
// bool goose_receiver_cleanup_alternative(void) {
//     pthread_mutex_lock(&goose_cleanup_mutex);
    
//     // Signal all threads to stop
//     running_Goose = false;
//     internal_shutdown_flag = true;
    
//     for (int i = 0; i < goose_instance_count; ++i) {
//         if (thread_data[i].receiver) {
//             GooseReceiver_stop(thread_data[i].receiver);
//         }
//     }
    
//     // Give threads time to finish gracefully
//     Thread_sleep(500);
    
//     // Cancel and join threads
//     if (threads != NULL) {
//         for (int i = 0; i < goose_instance_count; ++i) {
//             if (threads[i] != 0) {
//                 pthread_cancel(threads[i]);
//                 pthread_join(threads[i], NULL);
//             }
//         }
//         free(threads);
//         threads = NULL;
//     }
    
//     // Clean up resources
//     for (int i = 0; i < goose_instance_count; ++i) {
//         if (thread_data[i].receiver != NULL) {
//             GooseReceiver_destroy(thread_data[i].receiver);
//             thread_data[i].receiver = NULL;
//         }
//         if (thread_data[i].subscriber != NULL) {
//             GooseSubscriber_destroy(thread_data[i].subscriber);
//             thread_data[i].subscriber = NULL;
//         }
        
//         if (thread_data[i].interface) {
//             free(thread_data[i].interface);
//             thread_data[i].interface = NULL;
//         }
//         if (thread_data[i].GoCBRef) {
//             free(thread_data[i].GoCBRef);
//             thread_data[i].GoCBRef = NULL;
//         }
//         if (thread_data[i].DatSet) {
//             free(thread_data[i].DatSet);
//             thread_data[i].DatSet = NULL;
//         }
//         if (thread_data[i].MACAddress) {
//             free(thread_data[i].MACAddress);
//             thread_data[i].MACAddress = NULL;
//         }
        
//         memset(&thread_data[i], 0, sizeof(ThreadData));
//     }
    
//     if (thread_data) {
//         free(thread_data);
//         thread_data = NULL;
//     }
//     goose_instance_count = 0;
    
//     pthread_mutex_unlock(&goose_cleanup_mutex);
//     LOG_INFO("Goose_Listener", "GOOSE cleanup complete");
//     return SUCCESS;
// }


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
        // printf("gooose interface: %s\n", thread_data[i].interface);

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
        //  printf("GoCBRef: %s\n", thread_data[i].GoCBRef);

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
      thread_data[i].MACAddress = malloc(6 * sizeof(uint8_t));
if (!thread_data[i].MACAddress) {
    LOG_ERROR("Goose_Listener", "Memory allocation failed for MAC address");
    goto cleanup_init_failure;
}

if (!parse_mac_address(config[i].MACAddress, thread_data[i].MACAddress)) {
    LOG_ERROR("Goose_Listener", "Failed to parse MAC address");
    free(thread_data[i].MACAddress);
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

    // Set signal handler only once
    struct sigaction sa;
    sa.sa_handler = sigint_handler_Goose;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        LOG_ERROR("Goose_Listener", "Failed to set SIGINT handler: %s", strerror(errno));
        return FAIL;
    }

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
            LOG_INFO("Goose_Listener", "Created thread for instance %d", i);
        }
    }
    LOG_INFO("Goose_Listener", "Goose_Listener threads started.");
    printf("Goose_Listener threads started.\n");
    fflush(stdout);
    return all_threads_created;
}
