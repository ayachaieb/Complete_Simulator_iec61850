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
int goose_receiver_cleanup(void);
bool goose_receiver_is_running(void);
static pthread_t *threads = NULL;
static ThreadData *thread_data = NULL;
int goose_instance_count = 0;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static void
sigint_handler(int signalId)
{
    running_Goose = 0;
    StateMachine_push_event(STATE_EVENT_shutdown, NULL, NULL);
    internal_shutdown_flag = true; // Signal ipc_run_loop to exit

    if (SUCCESS == goose_receiver_cleanup())
    {
        printf("Goose_ListenerCleaning up thread successfully");
       // printf(" FASAKH BALIZZZZ\n");
        fflush(stdout);
    }
    else
    {
        printf("Failed to clean up GOOSE receiver.\n");
    }

    exit(0);
}

static void
gooseListener(GooseSubscriber subscriber, void *parameter)
{
    printf("GOOSE Event received!\n");

    pthread_mutex_lock(&log_mutex);
    static FILE *logFile = NULL;
    if (subscriber == NULL)
    {
        fprintf(stderr, "Error: Received NULL subscriber in gooseListener.\n");
        return;
    }
    if (logFile == NULL)
    {
        logFile = fopen("log.txt", "a");

        if (logFile == NULL)
        {
            fprintf(stderr, "CRITICAL: Failed to open log.txt: %s. Falling back to console.\n", strerror(errno));
            logFile = stdout;
        }
    }

    char timeStr[64];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    // Format ="YYYY-MM-DD HH:MM:SS".
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(logFile, "\n--- [%s] GOOSE Event Received ---\n", timeStr);

    fprintf(logFile, "  Message validity: %s\n", GooseSubscriber_isValid(subscriber) ? "valid" : "INVALID");
    fprintf(logFile, "  vlanTag: %s\n", GooseSubscriber_isVlanSet(subscriber) ? "found" : "NOT found");
    if (GooseSubscriber_isVlanSet(subscriber))
    {
        fprintf(logFile, "    vlanId: %u\n", GooseSubscriber_getVlanId(subscriber));
        fprintf(logFile, "    vlanPrio: %u\n", GooseSubscriber_getVlanPrio(subscriber));
    }
    fprintf(logFile, "  appId: %d\n", GooseSubscriber_getAppId(subscriber));

    uint8_t macBuf[6];
    GooseSubscriber_getSrcMac(subscriber, macBuf);
    fprintf(logFile, "  srcMac: %02X:%02X:%02X:%02X:%02X:%02X\n", macBuf[0], macBuf[1], macBuf[2], macBuf[3], macBuf[4], macBuf[5]);
    GooseSubscriber_getDstMac(subscriber, macBuf);
    fprintf(logFile, "  dstMac: %02X:%02X:%02X:%02X:%02X:%02X\n", macBuf[0], macBuf[1], macBuf[2], macBuf[3], macBuf[4], macBuf[5]);

    fprintf(logFile, "  goId: %s\n", GooseSubscriber_getGoId(subscriber));
    fprintf(logFile, "  goCbRef: %s\n", GooseSubscriber_getGoCbRef(subscriber));
    fprintf(logFile, "  dataSet: %s\n", GooseSubscriber_getDataSet(subscriber));
    fprintf(logFile, "  confRev: %u\n", GooseSubscriber_getConfRev(subscriber));
    fprintf(logFile, "  ndsCom: %s\n", GooseSubscriber_needsCommission(subscriber) ? "true" : "false");
    fprintf(logFile, "  simul: %s\n", GooseSubscriber_isTest(subscriber) ? "true" : "false");
    fprintf(logFile, "  stNum: %u sqNum: %u\n", GooseSubscriber_getStNum(subscriber),
            GooseSubscriber_getSqNum(subscriber));
    fprintf(logFile, "  timeToLive: %u\n", GooseSubscriber_getTimeAllowedToLive(subscriber));

    uint64_t timestamp = GooseSubscriber_getTimestamp(subscriber);

    fprintf(logFile, "  timestamp: %llu ms (approx %u.%03u seconds)\n",
            (long long unsigned int)timestamp,
            (uint32_t)(timestamp / 1000), (uint32_t)(timestamp % 1000));

    MmsValue *values = GooseSubscriber_getDataSetValues(subscriber);

    if (values != NULL)
    {
        char buffer[1024];
        MmsValue_printToBuffer(values, buffer, sizeof(buffer));
        fprintf(logFile, "  AllData: %s\n", buffer);
    }

    fprintf(logFile, "-----------------------------------------------------\n");

    // Flush
    fflush(logFile);
    pthread_mutex_unlock(&log_mutex);
}

int goose_receiver_cleanup(void)
{
    if (thread_data == NULL)
        return FAIL;

    printf("Cleaning up thread resources\n");
    // Loop 1: Destroying GooseReceiver objects (which implicitly destroy their GooseSubscriber objects)
    for (int i = 0; i < goose_instance_count; i++)
    {
        printf("Cleaning up receiver for instance %d ---- %d\n", i, goose_instance_count);
        if (thread_data[i].receiver != NULL)
        {
            printf("Cleaning up receiver\n");
            GooseReceiver_stop(thread_data[i].receiver);
            GooseReceiver_destroy(thread_data[i].receiver);
            thread_data[i].receiver = NULL;
        }
    }

    // Thread joining and freeing threads array
    if (threads != NULL)
    {
        for (int i = 0; i < goose_instance_count; i++)
        {
            if (pthread_join(threads[i], NULL) != 0)
            {
                LOG_ERROR("Goose_Listener", "Failed to join thread for instance %d", i);
            }
        }
        free(threads);
        threads = NULL;
    }
    printf("All threads joined successfully\n");
    return SUCCESS;
}

bool goose_receiver_is_running(void)
{
    return receiver != NULL;
}

void *goose_thread_task(void *arg)
{
    ThreadData *data = (ThreadData *)arg;

    signal(SIGINT, sigint_handler);
    if (data->receiver == NULL)
    {
        data->receiver = GooseReceiver_create();
    }
    if (data->receiver == NULL)
    {
        LOG_ERROR("Goose_Listener", "Failed to create GooseReceiver for appid 0x%d", data->goose_id);
        return NULL;
    }

    GooseReceiver_setInterfaceId(data->receiver, data->interface);

    // Use configurable GoCBRef and DatSet from ThreadData
    GooseSubscriber subscriber  = GooseSubscriber_create(data->GoCBRef, NULL);
    if (subscriber == NULL)
    {
        LOG_ERROR("Goose_Listener", "Failed to create GooseSubscriber for goappid 0x%d", data->goose_id);
        GooseReceiver_destroy(data->receiver);
        data->receiver = NULL;
        return NULL;
    }

    // Set configurable AppID
    GooseSubscriber_setAppId(subscriber, data->goose_id);

    // Set configurable destination MAC address
    GooseSubscriber_setDstMac(subscriber, data->MACAddress);
    GooseSubscriber_setListener(subscriber, gooseListener, NULL);
    GooseReceiver_addSubscriber(data->receiver, subscriber);
    printf(" GOOSE subscriber MAC address to: %02X:%02X:%02X:%02X:%02X:%02X GOOSE subscriber for %s interface=%s with AppId %u\n",
           data->MACAddress[0], data->MACAddress[1],
           data->MACAddress[2], data->MACAddress[3],
           data->MACAddress[4], data->MACAddress[5],data->GoCBRef,data->interface ,data->goose_id );
   
    GooseReceiver_start(data->receiver);

    if (!GooseReceiver_isRunning(data->receiver))
    {
        LOG_ERROR("Goose_Listener", "Failed to start GooseReceiver for appid 0x%d on interface %s",
                  data->goose_id, data->interface);
        GooseReceiver_destroy(data->receiver);
        data->receiver = NULL;
        subscriber = NULL;
        return NULL;
    }

    while ((running_Goose) && (!internal_shutdown_flag))
    {
        Thread_sleep(100);
    }

    LOG_INFO("Goose_Listener", "GOOSE subscriber thread for goappid 0x%d shutting down.", data->goose_id);
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

    if (threads != NULL || thread_data != NULL)
    {
        LOG_INFO("Goose_Listener", "Previous GOOSE Publisher instances found. Cleaning up before re-initialization.");

        if (threads)
        {
            free(threads);
            threads = NULL;
        }
        if (thread_data)
        {
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
    if (NULL == thread_data)
    {
        LOG_ERROR("Goose_Listener", "Memory allocation failed for thread_data!");
        free(threads); // Clean up threads array
        threads = NULL;
        return FAIL;
    }
    memset(thread_data, 0, goose_instance_count * sizeof(ThreadData));
    int i = 0;

    for (i = 0; i < goose_instance_count; i++)
    {
        memset(&thread_data[i], 0, sizeof(ThreadData));
        thread_data[i].receiver = NULL;
        thread_data[i].subscriber = NULL;
        if (config[i].appId)
        {
            char *endptr;
            unsigned long val = strtoul(config[i].appId, &endptr, 10);
            if (*endptr != '\0' || val > UINT32_MAX)
            {
                LOG_ERROR("Goose_Listener", "Invalid appId format or value for instance %d: %s", i, config[i].appId);
                goto cleanup_init_failure;
            }

            thread_data[i].AppID = (uint32_t)val;
            thread_data[i].goose_id = (uint32_t)strtoul(config[i].GOAppID, NULL, 10);
        }
        else
        {
            LOG_ERROR("Goose_Listener", "appId is NULL for instance %d", i);
            goto cleanup_init_failure;
        }

        if (config[i].Interface)
        {
            thread_data[i].interface = strdup(config[i].Interface);
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

        if (config[i].GoCBRef)
        {
            thread_data[i].GoCBRef = strdup(config[i].GoCBRef);
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

        LOG_INFO("Goose_Listener", "All thread_data initialized successfully");
    }
    return SUCCESS;

cleanup_init_failure:
    LOG_INFO("Goose_Listener", "cleanup_init_failure happening");
    // Free all memory allocated
    for (int j = 0; j <= i; ++j)
    {
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
        thread_data = NULL; // to avoid dangling pointer
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
    }

    LOG_INFO("Goose_Listener", "Goose_Listener threads started.");

    return all_threads_created;
}