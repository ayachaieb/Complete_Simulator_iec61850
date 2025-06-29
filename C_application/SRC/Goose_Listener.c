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
volatile sig_atomic_t running_Goose = 1;
extern volatile bool internal_shutdown_flag;
GooseReceiver receiver = NULL;
bool goose_receiver_cleanup(void);
bool goose_receiver_is_running(void);
static pthread_t *threads = NULL;
static ThreadData *thread_data = NULL;
int goose_instance_count = 0;
static void
sigint_handler(int signalId)
{
    running_Goose = 0;
    StateMachine_push_event(STATE_EVENT_shutdown, NULL, NULL);
    internal_shutdown_flag = true; // Signal ipc_run_loop to exit
    printf("Received SIGINT, shutting down...\n");
    goose_receiver_cleanup(); // Clean up GOOSE receiver resources
    printf("GOOSE receiver  cleaned up.\n");
    exit(0); // Exit the program
}

static void
gooseListener(GooseSubscriber subscriber, void *parameter)
{ printf("\n--- GOOSE Event Received ---\n");
    fflush(stdout);
   //  printf(" Message validity: %s\n", GooseSubscriber_isValid(subscriber) ? "valid" : "INVALID");
    LOG_INFO("Goose_Listener", "Message validity: %s",
             GooseSubscriber_isValid(subscriber) ? "valid" : "INVALID");
     // Rest of the existing code
  //  printf("  vlanTag: %s\n", GooseSubscriber_isVlanSet(subscriber) ? "found" : "NOT found");
    if (GooseSubscriber_isVlanSet(subscriber))
    {
      //  printf("    vlanId: %u\n", GooseSubscriber_getVlanId(subscriber));
       // printf("    vlanPrio: %u\n", GooseSubscriber_getVlanPrio(subscriber));
    }
  //  printf("  appId: %d\n", GooseSubscriber_getAppId(subscriber));
    uint8_t macBuf[6];
    GooseSubscriber_getSrcMac(subscriber,macBuf);
    printf("  srcMac: %02X:%02X:%02X:%02X:%02X:%02X\n", macBuf[0],macBuf[1],macBuf[2],macBuf[3],macBuf[4],macBuf[5]);
    GooseSubscriber_getDstMac(subscriber,macBuf);
    fflush(stdout);
  //  printf("  dstMac: %02X:%02X:%02X:%02X:%02X:%02X\n", macBuf[0],macBuf[1],macBuf[2],macBuf[3],macBuf[4],macBuf[5]);
    printf("  goId: %s\n", GooseSubscriber_getGoId(subscriber));
    printf("  goCbRef: %s\n", GooseSubscriber_getGoCbRef(subscriber));
    printf("  dataSet: %s\n", GooseSubscriber_getDataSet(subscriber));
    printf("  confRev: %u\n", GooseSubscriber_getConfRev(subscriber));
    printf("  ndsCom: %s\n", GooseSubscriber_needsCommission(subscriber) ? "true" : "false");
    printf("  simul: %s\n", GooseSubscriber_isTest(subscriber) ? "true" : "false");
    printf("  stNum: %u sqNum: %u\n", GooseSubscriber_getStNum(subscriber),
             GooseSubscriber_getSqNum(subscriber));
    printf("  timeToLive: %u\n", GooseSubscriber_getTimeAllowedToLive(subscriber));
fflush(stdout);
    uint64_t timestamp = GooseSubscriber_getTimestamp(subscriber);

    printf("  timestamp: %llu ms (approx %u.%03u seconds)\n",
           (long long unsigned int)timestamp,
           (uint32_t) (timestamp / 1000), (uint32_t) (timestamp % 1000));

    printf("  message is %s\n", GooseSubscriber_isValid(subscriber) ? "valid" : "INVALID");

    MmsValue* values = GooseSubscriber_getDataSetValues(subscriber);

    char buffer[1024];

    MmsValue_printToBuffer(values, buffer, 1024);

    printf("  AllData: %s\n", buffer);
    printf("--------------------------\n");
    fflush(stdout);
}

bool goose_receiver_cleanup(void)
{
    // Clean up GOOSE receiver resources
    if (receiver != NULL)
    {
        GooseReceiver_stop(receiver);
        GooseReceiver_destroy(receiver);
        receiver = NULL; // Set to NULL after destruction
    }
    printf("GOOSE receiver and subscriber cleaned up.\n");
    return SUCCESS;
}

bool goose_receiver_is_running_Goose(void)
{

    return receiver != NULL;
}

void *goose_thread_task(void *arg)
{
    ThreadData *data = (ThreadData *)arg;
    LOG_INFO("Goose_Listener", "Thread started for appid 0x%d on interface %s", data->AppID, data->interface);
LOG_INFO("Goose_Listener", "Thread started for appid 0x%d on interface %s", data->AppID, data->interface);
    LOG_INFO("Goose_Listener", "GoCBRef: %s, DatSet: %s, dstMac: %02x:%02x:%02x:%02x:%02x:%02x",
             data->GoCBRef, data->DatSet,
             data->MACAddress[0], data->MACAddress[1], data->MACAddress[2],
             data->MACAddress[3], data->MACAddress[4], data->MACAddress[5]);
    if (data->receiver == NULL) {
        data->receiver = GooseReceiver_create();
        if (data->receiver == NULL) {
            LOG_ERROR("Goose_Listener", "Failed to create GooseReceiver for appid 0x%d", data->AppID);
            return NULL;
        }
    }

    // Explicitly set the interface
    GooseReceiver_setInterfaceId(data->receiver, data->interface);
    LOG_INFO("Goose_Listener", "Set interface %s for appid 0x%d", data->interface, data->AppID);
   data->subscriber = GooseSubscriber_create("simpleIOGenericIO/LLN0$GO$gcbAnalogValues", 
                                         "simpleIOGenericIO/LLN0$AnalogValues");
GooseSubscriber_setAppId(data->subscriber, 1000);
uint8_t mac[] = {0x01, 0x0c, 0xcd, 0x01, 0x00, 0x01};
GooseSubscriber_setDstMac(data->subscriber, mac);
   
  //  data->subscriber = GooseSubscriber_create("IED1/LLN0$GO$gcbGoose1", "IED1/LLN0$DS$GooseDataSet1");
   // data->subscriber = GooseSubscriber_create(data->GoCBRef, data->DatSet);
    if (data->subscriber == NULL) {
        LOG_ERROR("Goose_Listener", "Failed to create GooseSubscriber for appid 0x%d", data->AppID);
        GooseReceiver_destroy(data->receiver);
        data->receiver = NULL;
        return NULL;
    }

    //GooseSubscriber_setAppId(data->subscriber, data->AppID);
   

   // GooseSubscriber_setDstMac(data->subscriber, data->MACAddress);
    GooseSubscriber_setListener(data->subscriber, gooseListener, NULL);
    GooseReceiver_addSubscriber(data->receiver, data->subscriber);

    GooseReceiver_start(data->receiver);
    if (!GooseReceiver_isRunning(data->receiver)) {
        LOG_ERROR("Goose_Listener", "Failed to start GooseReceiver for appid 0x%d on interface %s", 
                  data->AppID, data->interface);
        printf("Failed to start GOOSE subscriber on interface %s for appid 0x%d.\n", 
               data->interface, data->AppID);
        GooseReceiver_destroy(data->receiver);
        data->receiver = NULL;
        data->subscriber = NULL;
        return NULL;
    }

    while (running_Goose) {
        Thread_sleep(100);
    }

    LOG_INFO("Goose_Listener", "GOOSE subscriber thread for appid 0x%d shutting down.", data->AppID);
    GooseReceiver_stop(data->receiver);
    GooseReceiver_destroy(data->receiver);
    data->receiver = NULL;
    data->subscriber = NULL;

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
                if (thread_data[i].goose_id)
                    free(thread_data[i].goose_id);
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
        printf("gooose interface: %s\n", thread_data[i].interface);

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
        printf("GoCBRef: %s\n", thread_data[i].GoCBRef);

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
    signal(SIGINT, sigint_handler);
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
            printf("Created a goooooose   thread for instance %d\n", i);
            LOG_INFO("Goose_Listener", "Created thread for instance %d", i);
        }
    }
    // printf("All threads created successfully: %d\n", all_threads_created);
    LOG_INFO("Goose_Listener", "Goose_Listenerthreads started.");

    return all_threads_created;
    return 0;
}