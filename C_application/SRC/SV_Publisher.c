#include "SV_Publisher.h"
#include "hal_thread.h" // For Thread_sleep and Hal_getTimeInMs
#include "sv_publisher.h" // For SVPublisher API
#include "logger.h"       // Your custom logger module
#include <stdio.h>
#include <string.h>
#include <pthread.h>      // For threading
#include <errno.h>        // For strerror

// Internal state for the SV Publisher module
static SVPublisher g_svPublisher = NULL;
static bool g_sv_publisher_running = false;
static pthread_t g_sv_publisher_thread;
static char g_interface_name[32] = {0}; // Buffer for interface name

// Store ASDU handles and their data point indices
// These need to be accessible by both init and the publishing thread.
static SVPublisher_ASDU g_asdu1 = NULL;
static SVPublisher_ASDU g_asdu2 = NULL;
static int g_float1_idx, g_float2_idx, g_ts1_idx;
static int g_float3_idx, g_float4_idx, g_ts2_idx;

// Forward declaration for the publishing thread function
static void* sv_publishing_thread(void* arg);

/**
 * @brief Initializes the SV Publisher module.
 * This function sets up the internal SVPublisher instance and its ASDUs.
 * It must be called before SVPublisher_Module_start().
 *
 * @param interface_name The network interface name (e.g., "eth0") to use for publishing.
 * @return true if initialization is successful, false otherwise.
 */
bool SVPublisher_init(const char* interface_name)
{
    // If already initialized, just return true
    if (g_svPublisher != NULL) {
        LOG_INFO("SV_Publisher_Module", "SV Publisher already initialized.");
        return true;
    }

    // Validate interface name
    if (interface_name == NULL || strlen(interface_name) >= sizeof(g_interface_name)) {
        LOG_ERROR("SV_Publisher_Module", "Invalid interface name provided: %s", interface_name ? interface_name : "NULL");
        return false;
    }

    // Store interface name
    strncpy(g_interface_name, interface_name, sizeof(g_interface_name) - 1);
    g_interface_name[sizeof(g_interface_name) - 1] = '\0';

    LOG_INFO("SV_Publisher_Module", "Initializing SV Publisher on interface %s", g_interface_name);

    // Create the SV Publisher instance
    g_svPublisher = SVPublisher_create(NULL, g_interface_name);

    if (g_svPublisher == NULL) {
        LOG_ERROR("SV_Publisher_Module", "Failed to create SV publisher on interface %s", g_interface_name);
        return false;
    }

    /* Create first ASDU (Application Specific Data Unit) and add data points */
    g_asdu1 = SVPublisher_addASDU(g_svPublisher, "svpub1", NULL, 1);
    if (g_asdu1 == NULL) {
        LOG_ERROR("SV_Publisher_Module", "Failed to add ASDU 'svpub1'");
        SVPublisher_destroy(g_svPublisher); // Clean up partially created publisher
        g_svPublisher = NULL;
        return false;
    }
    g_float1_idx = SVPublisher_ASDU_addFLOAT(g_asdu1);
    g_float2_idx = SVPublisher_ASDU_addFLOAT(g_asdu1);
    g_ts1_idx = SVPublisher_ASDU_addTimestamp(g_asdu1);

    /* Create second ASDU and add data points */
    g_asdu2 = SVPublisher_addASDU(g_svPublisher, "svpub2", NULL, 1);
    if (g_asdu2 == NULL) {
        LOG_ERROR("SV_Publisher_Module", "Failed to add ASDU 'svpub2'");
        SVPublisher_destroy(g_svPublisher); // Clean up partially created publisher
        g_svPublisher = NULL;
        g_asdu1 = NULL; // Clear first ASDU handle as well
        return false;
    }
    g_float3_idx = SVPublisher_ASDU_addFLOAT(g_asdu2);
    g_float4_idx = SVPublisher_ASDU_addFLOAT(g_asdu2);
    g_ts2_idx = SVPublisher_ASDU_addTimestamp(g_asdu2);

    // Finalize publisher setup
    SVPublisher_setupComplete(g_svPublisher);

    LOG_INFO("SV_Publisher_Module", "SV Publisher initialization complete.");
    return true;
}

/**
 * @brief Starts the SV publishing loop in a separate thread.
 * This function should only be called after successful initialization.
 * The publisher will continuously send SV messages until stopped.
 *
 * @return true if the publishing thread is successfully started, false otherwise.
 */
bool SVPublisher_start()
{
    // Check if publisher is initialized
    if (g_svPublisher == NULL) {
        LOG_ERROR("SV_Publisher_Module", "SV Publisher not initialized. Call SVPublisher_Module_init first.");
        return false;
    }

    // Check if already running
    if (g_sv_publisher_running) {
        LOG_INFO("SV_Publisher_Module", "SV Publisher already running.");
        return true;
    }

    // Set running flag and create thread
    g_sv_publisher_running = true;
    if (pthread_create(&g_sv_publisher_thread, NULL, sv_publishing_thread, NULL) != 0) {
        LOG_ERROR("SV_Publisher_Module", "Failed to create SV publishing thread: %s", strerror(errno));
        g_sv_publisher_running = false; // Reset flag on failure
        return false;
    }

    LOG_INFO("SV_Publisher_Module", "SV Publisher thread started.");
    return true;
}

/**
 * @brief Stops the SV publishing loop and cleans up resources.
 * This function can be called to gracefully stop the publisher.
 * It waits for the publishing thread to terminate.
 */
void SVPublisher_stop()
{
    // If not running, nothing to do
    if (!g_sv_publisher_running) {
        LOG_INFO("SV_Publisher_Module", "SV Publisher is not running or already stopped.");
        return;
    }

    LOG_INFO("SV_Publisher_Module", "Stopping SV Publisher thread...");
    g_sv_publisher_running = false; // Signal thread to stop
    pthread_join(g_sv_publisher_thread, NULL); // Wait for thread to finish

    // Destroy the SV Publisher instance and clear handles
    if (g_svPublisher != NULL) {
        SVPublisher_destroy(g_svPublisher);
        g_svPublisher = NULL;
        g_asdu1 = NULL; // Clear ASDU handles
        g_asdu2 = NULL;
        LOG_INFO("SV_Publisher_Module", "SV Publisher destroyed.");
    }
}

/**
 * @brief The main loop for SV publishing, runs in a separate thread.
 * This function continuously updates and publishes Sampled Values.
 * @param arg Not used.
 * @return NULL (thread exit status).
 */
static void* sv_publishing_thread(void* arg)
{
    LOG_INFO("SV_Publisher_Module", "SV Publishing thread started its loop.");

    // Initial values for the floats
    float fVal1 = 1234.5678f;
    float fVal2 = 0.12345f;

    // Critical check: ensure publisher and ASDU handles are valid before entering loop
    if (g_svPublisher == NULL || g_asdu1 == NULL || g_asdu2 == NULL) {
        LOG_ERROR("SV_Publisher_Module", "SV Publisher or ASDU handles are NULL in publishing thread. Aborting.");
        return NULL;
    }

    // Main publishing loop
    while (g_sv_publisher_running) {
        Timestamp ts;
        Timestamp_clearFlags(&ts);
        Timestamp_setTimeInMilliseconds(&ts, Hal_getTimeInMs());

        /* Update the values in the SV ASDUs */
        SVPublisher_ASDU_setFLOAT(g_asdu1, g_float1_idx, fVal1);
        SVPublisher_ASDU_setFLOAT(g_asdu1, g_float2_idx, fVal2);
        SVPublisher_ASDU_setTimestamp(g_asdu1, g_ts1_idx, ts);

        SVPublisher_ASDU_setFLOAT(g_asdu2, g_float3_idx, fVal1 * 2);
        SVPublisher_ASDU_setFLOAT(g_asdu2, g_float4_idx, fVal2 * 2);
        SVPublisher_ASDU_setTimestamp(g_asdu2, g_ts2_idx, ts);

        /* Update the sample counters */
        SVPublisher_ASDU_increaseSmpCnt(g_asdu1);
        SVPublisher_ASDU_increaseSmpCnt(g_asdu2);

        // Increment values for next sample
        fVal1 += 1.1f;
        fVal2 += 0.1f;

        /* Send the SV message */
        SVPublisher_publish(g_svPublisher);

        // Sleep for a short duration to control sample rate
        // This sleep time should be adjusted for real applications to match the SV sample rate!
        Thread_sleep(50); // 50ms sleep -> 20 samples/second
    }

    LOG_INFO("SV_Publisher_Module", "SV Publishing thread finished its loop.");
    return NULL;
}