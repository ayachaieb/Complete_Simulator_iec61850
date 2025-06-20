#define _GNU_SOURCE
#include "SV_Publisher.h"
#include "hal_thread.h" // For Thread_sleep and Hal_getTimeInMs
#include "sv_publisher.h" // For SVPublisher API
#include "logger.h"       // Your custom logger module
#include <stdio.h>
#include <string.h>
#include <pthread.h>      // For threading
#include <errno.h>        // For strerror
#include "util.h"        // For utility functions like Hal_getTimeInMs
#include <signal.h>       // For signal handling
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/syscall.h>
#include "iec61850_server.h"
#include "hal_thread.h"
#include "goose_receiver.h"
#include "goose_subscriber.h"
#include "mms_value.h" // For MmsValue_printToBuffer if needed
#include <sched.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/time.h>
#include "parser.h"
#include <unistd.h> // For sleep()
#include "util.h" 
// Internal state for the SV Publisher module

// CommParameters parameters = {0, 0, 0x5000, {0x01, 0x0C, 0xCD, 0x01, 0x00, 0x01}};

#define COM_VDPA_NB_ECH_PAR_SV 2
#define FREQ_EN_HZ (f32)50.
#define PI (f32)3.1415926536
#define MAX_PHASES 150
/* 2400 for 2 samples and 4800 for 1 sample*/
#define SAMPLES_PER_SECOND 2400

/* Sampling time defined as 208 microseconds */
#define SAMPLING_TIME_208US (float)0.000208f
#define DELAY_416US (float)(SAMPLING_TIME_208US * 2.f)
#define DELAY_208US 208.0f
#define LOOPS_PER_CYCLE (uint32_t)((1.0f / SIGNAL_FREQUENCY) / SAMPLING_TIME_208US)

/* Signal frequency (for example 50 Hz) */
#define SIGNAL_FREQUENCY (float)50.0f

/* Constants for RMS values */
#define OMT_VEFF_RMS_VOLTAGE (float)8.0f
#define OMT_VEFF_RMS_CURRENT (float)0.0f
#define OMT_STPM_VAL_RMS_1V (float)1.0f
#define OMT_STPM_VAL_RMS_1A (float)1.0f

/* Constants for I4 and I5 */
#define CONSTANT_I4 7.0f
#define CONSTANT_I5 8.0f
#define COM_VDPA_PERIODE_SV_EN_NS 416667 // 2*208.33 us (Freq 4800Hz -> 1 ech toute les 208.33 us)
#define COM_VDPA_CADENCE_ECH_EN_US (f32)(COM_VDPA_PERIODE_SV_EN_NS / COM_VDPA_NB_ECH_PAR_SV / 1000.)

typedef float f32;
typedef f32 float32_t; // Pour compatibilite ancienne version

enum
{
    COM_VDPA_ECH_DATA_IND_I1,
    COM_VDPA_ECH_DATA_IND_I1Q,
    COM_VDPA_ECH_DATA_IND_I2,
    COM_VDPA_ECH_DATA_IND_I2Q,
    COM_VDPA_ECH_DATA_IND_I3,
    COM_VDPA_ECH_DATA_IND_I3Q,
    COM_VDPA_ECH_DATA_IND_I4,
    COM_VDPA_ECH_DATA_IND_I4Q,
    COM_VDPA_ECH_DATA_IND_V1,
    COM_VDPA_ECH_DATA_IND_V1Q,
    COM_VDPA_ECH_DATA_IND_V2,
    COM_VDPA_ECH_DATA_IND_V2Q,
    COM_VDPA_ECH_DATA_IND_V3,
    COM_VDPA_ECH_DATA_IND_V3Q,
    COM_VDPA_ECH_DATA_IND_V4,
    COM_VDPA_ECH_DATA_IND_V4Q,
    COM_VDPA_NB_DATA_PAR_ECH
};

typedef struct
{
    float channel1_voltage[3];
    float channel1_current[3];
    int duration_ms;
} PhaseSettings;

/* GOOSE Subscriber Code Integration */

/* Track old stNum to detect changes */

static uint32_t oldStNum = 0;
/* Variables for latency measurement */

static bool isMeasuring = false;
static bool latencyMeasured = false;
static uint32_t sampleCount = 0;
static int current_phase = 0;
static uint64_t phase_start_tick = 0;
static uint64_t phase_duration_ticks = 0;
static uint8_t end_test = 0;
static uint64_t faultStartTimeNs = 0;

static f32 angleCrs = (f32)0.;
static f32 pasCrs = FREQ_EN_HZ * (f32)360. * COM_VDPA_CADENCE_ECH_EN_US / (f32)1000000.;



PhaseSettings phases[MAX_PHASES];
int phase_count = 0;

uint64_t tick_208_us = 0;
volatile sig_atomic_t running = 1;
extern volatile bool internal_shutdown_flag; // Declare as extern
/* Number of loops incremented for each sample sent (tracking time in 256us steps) */
static uint32_t sNbLoop208us = 0;
/* Variables for voltages and currents */
static int channel1_voltage1, channel1_voltage2, channel1_voltage3;
static int channel1_current1, channel1_current2, channel1_current3;

static int tbIndData[COM_VDPA_NB_ECH_PAR_SV][COM_VDPA_NB_DATA_PAR_ECH];

typedef struct
{
    uint16_t GOOSEappId; // app id svpub
    char *svInterface;
    const char *scenarioConfigFile;
    char **svIDs;
    CommParameters parameters;
    SVPublisher svPublisher;
    SVPublisher_ASDU asdu1;
    SVPublisher_ASDU asdu2;
    SVPublisher_ASDU asdu;
    GooseReceiver gooseReceiver;
    GooseSubscriber gooseSubscriber;
    timer_t timerid;
    char *goCbRef;

} ThreadData;


int instance_count = 0;
static pthread_t *threads = NULL;
static ThreadData *thread_data = NULL;

static float fOmtStpmSimuGetVal(float veff, float freq, float phi, uint16_t harmoniques);
// Forward declaration for the publishing thread function
static void* sv_publishing_thread(void* arg);




void sigint_handler(int sig) {
    LOG_INFO("SV_Publisher", "Received Ctrl+C, shutting down...");
    running = false;
    SVPublisher_stop();
    StateMachine_push_event(STATE_EVENT_shutdown, NULL, NULL);
    internal_shutdown_flag = true; // Signal ipc_run_loop to exit
}
void timer_handler(int signum, siginfo_t *si, void *uc)

{
    ThreadData *current_data = (ThreadData *)si->si_value.sival_ptr;
    if (current_data == NULL)
    {
        printf("Timer handler: current_data is NULL\n");
        return;
    }
    //   printf("Timer handler for appid  0x%04x\n", current_data->parameters.appId);

    static __thread int sample = 0;
    bool faultCondition = false;
    Quality q = QUALITY_VALIDITY_GOOD;
    SVPublisher_ASDU asdu;
    if (current_data && running)
    {
        for (int sample = 0; sample < COM_VDPA_NB_ECH_PAR_SV; sample++)
        {
            if (0 == sample)
            {
                asdu = current_data->asdu1;
            }
            else
            {
                asdu = current_data->asdu2;
            }
#ifdef SINU_METHOD_ANA
            /* Calculate instantaneous values and send samples */
            channel1_voltage1 = (int)(100.f * fComStpmSimuGetVal(phases[current_phase].channel1_voltage[0], 0.0f));   // V1
            channel1_voltage2 = (int)(100.f * fComStpmSimuGetVal(phases[current_phase].channel1_voltage[1], 240.0f)); // V2
            channel1_voltage3 = (int)(100.f * fComStpmSimuGetVal(phases[current_phase].channel1_voltage[2], 120.0f)); // V3

            channel1_current1 = (int)(1000.f * fComStpmSimuGetVal(phases[current_phase].channel1_current[0], 0.0f));   // I1
            channel1_current2 = (int)(1000.f * fComStpmSimuGetVal(phases[current_phase].channel1_current[1], 240.0f)); // I2
            channel1_current3 = (int)(1000.f * fComStpmSimuGetVal(phases[current_phase].channel1_current[2], 120.0f)); // I3
#else
            channel1_voltage1 = (int)(100.f * fOmtStpmSimuGetVal(phases[current_phase].channel1_voltage[0], FREQ_EN_HZ, 0.0f, 0));   // V1
            channel1_voltage2 = (int)(100.f * fOmtStpmSimuGetVal(phases[current_phase].channel1_voltage[1], FREQ_EN_HZ, 240.0f, 0)); // V2
            channel1_voltage3 = (int)(100.f * fOmtStpmSimuGetVal(phases[current_phase].channel1_voltage[2], FREQ_EN_HZ, 120.0f, 0)); // V3

            channel1_current1 = (int)(1000.f * fOmtStpmSimuGetVal(phases[current_phase].channel1_current[0], FREQ_EN_HZ, 0.0f, 0));   // I1
            channel1_current2 = (int)(1000.f * fOmtStpmSimuGetVal(phases[current_phase].channel1_current[1], FREQ_EN_HZ, 240.0f, 0)); // I2
            channel1_current3 = (int)(1000.f * fOmtStpmSimuGetVal(phases[current_phase].channel1_current[2], FREQ_EN_HZ, 120.0f, 0)); // I3
#endif
            /*if(current_phase == 1)
            {
                printf("[%d] V1 %d V2 %d V3 %d \n\r",sNbLoop208us,channel1_voltage1,channel1_voltage2,channel1_voltage3);
                printf("[%d] I1 %d I2 %d I3 %d \n\r",sNbLoop208us,channel1_current1,channel1_current2,channel1_current3);
            }*/
            SVPublisher_ASDU_setINT32(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_I1], channel1_current1);
            SVPublisher_ASDU_setQuality(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_I1Q], q);

            SVPublisher_ASDU_setINT32(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_I2], channel1_current2);
            SVPublisher_ASDU_setQuality(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_I2Q], q);

            SVPublisher_ASDU_setINT32(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_I3], channel1_current3);
            SVPublisher_ASDU_setQuality(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_I3Q], q);

            SVPublisher_ASDU_setINT32(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_I4], channel1_current1 + channel1_current2 + channel1_current3);
            SVPublisher_ASDU_setQuality(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_I4Q], q);

            SVPublisher_ASDU_setINT32(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_V1], channel1_voltage1);
            SVPublisher_ASDU_setQuality(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_V1Q], q);

            SVPublisher_ASDU_setINT32(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_V2], channel1_voltage2);
            SVPublisher_ASDU_setQuality(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_V2Q], q);

            SVPublisher_ASDU_setINT32(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_V3], channel1_voltage3);
            SVPublisher_ASDU_setQuality(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_V3Q], q);

            SVPublisher_ASDU_setINT32(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_V4], channel1_voltage1 + channel1_voltage2 + channel1_voltage3);
            SVPublisher_ASDU_setQuality(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_V4Q], q);

            // printf("Channel 1 - Voltage1: %d, Voltage2: %d, Voltage3: %d\n", channel1_voltage1, channel1_voltage2, channel1_voltage3);
            // printf("Channel 1 - Current1: %d, Current2: %d, Current3: %d\n", channel1_current1, channel1_current2, channel1_current3);

#ifdef SINU_METHOD_ANA
            angleCrs += pasCrs;
            if (angleCrs > (f32)360.)
            {
                angleCrs -= (f32)360.;
            }
#endif
            /* Latency measurement logic */
            /* Detect when current > 1 and start measurement if not started yet */
            faultCondition = (phases[current_phase].channel1_current[0] > 1.0f || phases[current_phase].channel1_current[1] > 1.0f || phases[current_phase].channel1_current[2] > 1.0f);

            /* If the current returns to exactly 1.0, reset measuring state */
            bool resetCondition =
                (phases[current_phase].channel1_current[0] == 1.0f) &&
                (phases[current_phase].channel1_current[1] == 1.0f) &&
                (phases[current_phase].channel1_current[2] == 1.0f);

            if (resetCondition && isMeasuring)
            {
                isMeasuring = false;
                latencyMeasured = false;
            }

            tick_208_us++;
            if ((current_phase < phase_count) && (end_test == 0))
            {
                if ((tick_208_us - phase_start_tick) >= phase_duration_ticks)
                {
                    // printf("[time %u ms] Passing Phase from %d -> %d \n\r", phases[current_phase].duration_ms, current_phase, current_phase + 1);
                    current_phase++;
                    phase_start_tick = tick_208_us;
                    phase_duration_ticks = phases[current_phase].duration_ms * 1000 / (unsigned int)DELAY_208US;
                }

                if (current_phase == phase_count)
                {
                    end_test = 1;
                    current_phase = phase_count - 1;
                }
            }

            sNbLoop208us++;

            if (sNbLoop208us >= LOOPS_PER_CYCLE)
            {
                sNbLoop208us -= LOOPS_PER_CYCLE;
            }
            SVPublisher_ASDU_setSmpCnt(asdu, sampleCount);
            SVPublisher_ASDU_setRefrTmNs(asdu, Hal_getTimeInNs());
            sampleCount = (sampleCount + 1) % SAMPLES_PER_SECOND;

            if (running)
            {
                SVPublisher_publish(current_data->svPublisher);
                // printf("Published from appid 0x%04x\n", current_data->parameters.appId);
            }
        }

        if (faultCondition && !isMeasuring)
        {
            isMeasuring = true;
            latencyMeasured = false;
            faultStartTimeNs = Hal_getTimeInNs();
            // printf("Fault detected, start measuring latency...\n");
        }
    }
}

static f32 fComStpmSimuGetVal(f32 veff, f32 phi)
{
    f32 theta;

    theta = angleCrs + phi; // Angle en degre
    if (theta > (f32)360.)
    {
        theta -= (f32)360.;
    }
    theta = theta * 2. * PI / (f32)360.; // Angle en radians
    return (veff * 1.41421356 * sin(theta));
}

/* Function to simulate the STPM values based on input */
static float fOmtStpmSimuGetVal(float veff, float freq, float phi, uint16_t harmoniques)
{
    float fsin;
    float fcos;
    float theta;

    if (0U == harmoniques)
    {
        theta = (freq / 1000000.0f) * (float)sNbLoop208us * DELAY_208US;
        theta -= (uint32_t)theta;
        theta *= 360.0f;
        theta += phi;
        if (theta > 360.0f)
        {
            theta -= 360.0f;
        }
        fComCalSinCos(theta, &fsin, &fcos);
    }
    else
    {
        float fsinh;
        fsin = 0.0f;
        for (uint8_t ind_rang = 0U; ind_rang < 16; ind_rang++)
        {
            if (0U != ((1 << ind_rang) & harmoniques))
            {
                theta = ((freq * (ind_rang + 1)) / 1000000.0f) * (float)sNbLoop208us * DELAY_208US;
                theta -= (uint32_t)theta;
                theta *= 360.0f;
                theta += phi;
                if (theta > 360.0f)
                {
                    theta -= 360.0f;
                }
                fComCalSinCos(theta, &fsinh, &fcos);
                fsin += fsinh;
            }
        }
    }
    return (veff * 1.41421356f * fsin);
}

static void setupSVPublisher(ThreadData *data)
{
   
     data->asdu1 = SVPublisher_addASDU(data->svPublisher, data->svIDs, NULL, 1);
     data->asdu2 = SVPublisher_addASDU(data->svPublisher, "svtesting", NULL, 1);

    for (uint8_t no_ech = 0U; no_ech < COM_VDPA_NB_ECH_PAR_SV; no_ech++)
    {
        if (0 == no_ech)
        {
            data->asdu = data->asdu1;
        }
        else
        {
            data->asdu = data->asdu2;
        }

        for (uint8_t no_data = 0U; no_data < COM_VDPA_NB_DATA_PAR_ECH; no_data++)
        {
            if ((no_data & 0x01) == 0)
            {
                tbIndData[no_ech][no_data] = SVPublisher_ASDU_addINT32(data->asdu);
            }
            else
            {
                tbIndData[no_ech][no_data] = SVPublisher_ASDU_addQuality(data->asdu);
            }
        }

        SVPublisher_ASDU_setSmpCntWrap(data->asdu, SAMPLES_PER_SECOND);
        SVPublisher_ASDU_setRefrTm(data->asdu, 0);
    }
    SVPublisher_setupComplete(data->svPublisher);

    // else
    // {
    //     printf("Failed to create SVPublisher for appid %u\n", data->parameters.appId);
    // }
}

int loadScenarioFile(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Could not open scenario file");
        return -1;
    }

    for (int i = 0; i < MAX_PHASES; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            phases[i].channel1_voltage[j] = 0.0f;
            phases[i].channel1_current[j] = 0.0f;
        }
        phases[i].duration_ms = 0;
    }

    char line[256];
    int current_phase = -1;

    while (fgets(line, sizeof(line), file) && current_phase < MAX_PHASES - 1)
    {
        if (strstr(line, "# Phase"))
        {
            current_phase++;
        }
        else if (current_phase >= 0)
        {
            float value;
            int duration;

            if (sscanf(line, "duration_ms=%d", &duration) == 1)
            {
                phases[current_phase].duration_ms = duration;
            }
            if (sscanf(line, "channel1_voltage1=%f", &value) == 1)
            {
                phases[current_phase].channel1_voltage[0] = value;
            }
            if (sscanf(line, "channel1_voltage2=%f", &value) == 1)
            {
                phases[current_phase].channel1_voltage[1] = value;
            }
            if (sscanf(line, "channel1_voltage3=%f", &value) == 1)
            {
                phases[current_phase].channel1_voltage[2] = value;
            }
            if (sscanf(line, "channel1_current1=%f", &value) == 1)
            {
                phases[current_phase].channel1_current[0] = value;
            }
            if (sscanf(line, "channel1_current2=%f", &value) == 1)
            {
                phases[current_phase].channel1_current[1] = value;
            }
            if (sscanf(line, "channel1_current3=%f", &value) == 1)
            {
                phases[current_phase].channel1_current[2] = value;
            }
        }
    }

    phase_count = current_phase + 1;
    fclose(file);

    return 0;
}

/* GOOSE listener callback */
static void gooseListener(GooseSubscriber subscriber, void *parameter)
{
    uint32_t newStNum = GooseSubscriber_getStNum(subscriber);

    /* Only if stNum changed */
    if (newStNum != oldStNum)
    {
        oldStNum = newStNum;

        /* If we are currently measuring (fault active) and haven't measured latency yet */
        if (isMeasuring && !latencyMeasured)
        {
            uint64_t now = Hal_getTimeInNs();
            uint64_t latency = now - faultStartTimeNs;
            printf("Latency measured: %f ms (stNum changed to %u)\n", ((float)latency / 1000000.f), newStNum);
            latencyMeasured = true;
        }
    }
}

static int setupGooseSubscriber(ThreadData *data)
{
    data->gooseReceiver = GooseReceiver_create();
    if (data->gooseReceiver == NULL)
    {
        printf("Failed to create GooseReceiver\n");
        return -1;
    }

    GooseReceiver_setInterfaceId(data->gooseReceiver, data->svInterface);

    data->gooseSubscriber = GooseSubscriber_create(data->goCbRef, NULL);
    if (data->gooseSubscriber == NULL)
    {
        printf("Failed to create GooseSubscriber\n");
        GooseReceiver_destroy(data->gooseReceiver);
        return -1;
    }
    printf("dstMac: %02x:%02x:%02x:%02x:%02x:%02x\n",
           data->parameters.dstAddress[0], data->parameters.dstAddress[1], data->parameters.dstAddress[2],
           data->parameters.dstAddress[3], data->parameters.dstAddress[4], data->parameters.dstAddress[5]);
    //  uint8_t dstMac[6] = {0x01, 0x0c, 0xcd, 0x01, 0x10, 0x08};
    GooseSubscriber_setDstMac(data->gooseSubscriber, data->parameters.dstAddress);

    printf("GOOSE appid 0x%04x\n", data->GOOSEappId);
    GooseSubscriber_setAppId(data->gooseSubscriber, data->GOOSEappId);
    GooseSubscriber_setListener(data->gooseSubscriber, gooseListener, NULL);
    GooseReceiver_addSubscriber(data->gooseReceiver, data->gooseSubscriber);

    GooseReceiver_start(data->gooseReceiver);

    if (!GooseReceiver_isRunning(data->gooseReceiver))
    {
        printf("Failed to start GOOSE subscriber. Root permission or correct interface required.\n");
        GooseReceiver_destroy(data->gooseReceiver);
        return -1;
    }

    return 0;
}


void *thread_task(void *arg)
{
    ThreadData *data = (ThreadData *)arg;

    printf("Thread started for appid 0x%04x on interface %s\n", data->parameters.appId, data->svInterface);
    printf("svInterface %s\nappid 0x%04x\ndstMac: %02x:%02x:%02x:%02x:%02x:%02x\n",
           data->svInterface, data->parameters.appId,
           data->parameters.dstAddress[0], data->parameters.dstAddress[1], data->parameters.dstAddress[2],
           data->parameters.dstAddress[3], data->parameters.dstAddress[4], data->parameters.dstAddress[5]);


    data->parameters.vlanPriority = 0;
   
    data->svPublisher = SVPublisher_create(&data->parameters, data->svInterface);
    if (!data->svPublisher)
    {
        printf("Failed to create SVPublisher for appid %u\n", data->parameters.appId);
        goto cleanup_on_error;
    }
    if (loadScenarioFile(data->scenarioConfigFile) != 0)
    {
        printf("Erreur loading scenario file\n");
        goto cleanup_on_error;
    }
    printf("phase_count = %u\n", phase_count);

    // Optional: Comment out GOOSE if not needed, or fix with correct interface
    if (setupGooseSubscriber(data) != 0)
    {
        printf("Failed to setup GOOSE Subscriber\n");
        goto cleanup_on_error;
    }

    setupSVPublisher(data);
    if (!data->svPublisher)
    {
        printf("Failed to create SVPublisher\n");
        goto cleanup_on_error;
    }


    phase_start_tick = tick_208_us;
    phase_duration_ticks = phases[current_phase].duration_ms * 1000 / (unsigned int)DELAY_208US;
   

    setup_timer(data); // Start periodic publishing
    // Run indefinitely until Ctrl+C
    while (running)
    {
        sleep(1); // Sleep to reduce CPU usage; timer_handler does the publishing
    }
    LOG_INFO("SV_Publisher", "Thread for appid %p gracefully shutting down.", data->parameters.appId);
    // --- Unconditional Cleanup when thread exits its loop ---
    if (data->svPublisher)
    {
        SVPublisher_destroy(data->svPublisher);
        data->svPublisher = NULL;
    }
    if (data->gooseReceiver)
    {
        GooseReceiver_stop(data->gooseReceiver);
        GooseReceiver_destroy(data->gooseReceiver);
        data->gooseReceiver = NULL;
    }
    // Free strdup'd strings within this ThreadData instance
    // These were allocated in SVPublisher_init and are part of this thread's data
    if (data->svInterface) free(data->svInterface);
    if (data->goCbRef) free(data->goCbRef);
    if (data->scenarioConfigFile) free(data->scenarioConfigFile);
    if (data->svIDs) free(data->svIDs);

    printf("Thread for appid 0x%04x finished publishing\n", data->parameters.appId);
    return NULL;

cleanup_on_error:
    // This block handles cleanup only if an error occurred during initialization
    if (data->svPublisher)
    {
        SVPublisher_destroy(data->svPublisher);
        data->svPublisher = NULL;
    }
    if (data->gooseReceiver)
    {
        GooseReceiver_stop(data->gooseReceiver);
        GooseReceiver_destroy(data->gooseReceiver);
        data->gooseReceiver = NULL;
    }
    // Free strdup'd strings here as well if they were allocated before the error
    if (data->svInterface) free(data->svInterface);
    if (data->goCbRef) free(data->goCbRef);
    if (data->scenarioConfigFile) free(data->scenarioConfigFile);
    if (data->svIDs) free(data->svIDs);

    printf("Thread for appid 0x%04x failed initialization and finished cleanup\n", data->parameters.appId);
    return NULL;
}


bool SVPublisher_init(SV_SimulationConfig* instances, int number_publishers)
{
    LOG_INFO("SV_Publisher", "Starting VDPA SV Publisher");
    LOG_INFO("SV_Publisher", "UID: %d", getuid());

    if (instances == NULL || number_publishers <= 0) {
        LOG_ERROR("SV_Publisher", "Invalid input: instances array is NULL or number_publishers is non-positive.");
        return FAIL;
    }
    
    // Clean up any previous allocations if init is called multiple times without cleanup
    if (threads != NULL || thread_data != NULL) {
        LOG_INFO("SV_Publisher", "Previous SV Publisher instances found. Cleaning up before re-initialization.");
        // Call a cleanup function here if one exists, otherwise free directly
        // For now, we'll assume SVPublisher_cleanup will be called externally or handle this.
        // For safety, we'll free here if not handled by a dedicated cleanup.
        // SVPublisher_cleanup(); // If this function exists and handles global cleanup
        // For this iteration, we'll just free the top-level arrays if they exist
        if (threads) { free(threads); threads = NULL; }
        if (thread_data) { 
            // Need to free internal strings within thread_data if they were strdup'd
            for (int i = 0; i < instance_count; ++i) {

                if (thread_data[i].svInterface) free(thread_data[i].svInterface);
                if (thread_data[i].goCbRef) free(thread_data[i].goCbRef);
                if (thread_data[i].scenarioConfigFile) free(thread_data[i].scenarioConfigFile);
                if (thread_data[i].svIDs) free(thread_data[i].svIDs);
            }
            free(thread_data); 
            thread_data = NULL; 
        }
        instance_count = 0;
    }

    instance_count = number_publishers;

    threads = (pthread_t *)malloc(instance_count * sizeof(pthread_t));
    if (!threads) {
        LOG_ERROR("SV_Publisher", "Memory allocation failed for threads!");
        return FAIL;
    }
    memset(threads, 0, instance_count * sizeof(pthread_t)); // Initialize to 0

    thread_data = (ThreadData *)malloc(instance_count * sizeof(ThreadData));
    if (!thread_data) {
        LOG_ERROR("SV_Publisher", "Memory allocation failed for thread_data!");
        free(threads); // Clean up threads array
        threads = NULL;
        return FAIL;
    }
    memset(thread_data, 0, instance_count * sizeof(ThreadData)); // Initialize to 0
int i=0;
//LOG_INFO("SV_Publisher", "Initializing %d SV Publisher instances", instance_count);
    for (i = 0; i < instance_count; i++) {
        // Initialize thread_data[i] to ensure all pointers are NULL before strdup
        memset(&thread_data[i], 0, sizeof(ThreadData));

        thread_data[i].parameters.vlanPriority = 0; // Default or get from config if available
        thread_data[i].parameters.vlanId = 0;       // Default or get from config if available
      if (instances[i].appId) {
            char *endptr;
            unsigned long val = strtoul(instances[i].appId, &endptr, 10); // Base 10 for numeric appId
            if (*endptr != '\0' || val > UINT32_MAX) {
                LOG_ERROR("SV_Publisher", "Invalid appId format or value for instance %d: %s", i, instances[i].appId);
                goto cleanup_init_failure;
            }
  
            // If you need to convert appId to an integer, do it here:
          //  thread_data[i].GOOSEappId = 0x5000 + atoi(instances[i].appId); 
            // For now, GOOSEappId is also a string from JSON, so we'll assume it's handled elsewhere or convert it.
            // Let's assume GOOSEappId is derived from appId string, so it should be uint32_t
            thread_data[i].parameters.appId = val; // Store the numeric value directly
            thread_data[i].GOOSEappId = (uint32_t)strtoul(instances[i].appId, NULL, 10); // Convert string appId to uint32_t
        } else {
            LOG_ERROR("SV_Publisher", "appId is NULL for instance %d", i);
            goto cleanup_init_failure;
        }

        if (instances[i].svInterface) {
            thread_data[i].svInterface = strdup(instances[i].svInterface);
            if (!thread_data[i].svInterface) {
                LOG_ERROR("SV_Publisher", "Memory allocation failed for svInterface for instance %d", i);
                goto cleanup_init_failure;
            }
        } else {
            LOG_ERROR("SV_Publisher", "svInterface is NULL for instance %d", i);
            goto cleanup_init_failure;
        }

        // Assuming goCbRef is a fixed string or can be derived
        thread_data[i].goCbRef = strdup("goose_subscriber"); // Example: fixed string
        if (!thread_data[i].goCbRef) {
            LOG_ERROR("SV_Publisher", "Memory allocation failed for goCbRef for instance %d", i);
            goto cleanup_init_failure;
        }

        if (instances[i].scenarioConfigFile) {
            thread_data[i].scenarioConfigFile = strdup(instances[i].scenarioConfigFile);
            if (!thread_data[i].scenarioConfigFile) {
                LOG_ERROR("SV_Publisher", "Memory allocation failed for scenarioConfigFile for instance %d", i);
                goto cleanup_init_failure;
            }
        } else {
            LOG_ERROR("SV_Publisher", "scenarioConfigFile is NULL for instance %d", i);
            goto cleanup_init_failure;
        }
        LOG_INFO("SV_Publisher", "scenarioConfigFile: %s", thread_data[i].scenarioConfigFile);
        if (instances[i].svIDs) {
            thread_data[i].svIDs = strdup(instances[i].svIDs);
            if (!thread_data[i].svIDs) {
                LOG_ERROR("SV_Publisher", "Memory allocation failed for svIDs for instance %d", i);
                goto cleanup_init_failure;
            }
        } else {
            LOG_ERROR("SV_Publisher", "svIDs is NULL for instance %d", i);
            goto cleanup_init_failure;
        }
        LOG_INFO("SV_Publisher", "svIDs: %s", thread_data[i].svIDs);
        // Parse and copy MAC address
        if (instances[i].dstMac) {
            if (!parse_mac_address(instances[i].dstMac, thread_data[i].parameters.dstAddress)) {
                LOG_ERROR("SV_Publisher", "Failed to parse MAC address for instance %d: %s", i, instances[i].dstMac);
                goto cleanup_init_failure;
            }
        } else {
            LOG_ERROR("SV_Publisher", "dstMac is NULL for instance %d", i);
            goto cleanup_init_failure;
        }
        LOG_INFO("SV_Publisher", "dstMac: %02x:%02x:%02x:%02x:%02x:%02x",
                 thread_data[i].parameters.dstAddress[0], thread_data[i].parameters.dstAddress[1],
                 thread_data[i].parameters.dstAddress[2], thread_data[i].parameters.dstAddress[3],
                 thread_data[i].parameters.dstAddress[4], thread_data[i].parameters.dstAddress[5]);
    }
LOG_INFO("SV_Publisher","outtaa");
    return SUCCESS;
LOG_INFO("SV_Publisher","cleanup happening");
cleanup_init_failure:
    // Free all memory allocated so far for thread_data and threads
    for (int j = 0; j <= i; ++j) { // Free up to the current failed instance
        if (thread_data[j].parameters.appId) free(thread_data[j].parameters.appId);
        if (thread_data[j].svInterface) free(thread_data[j].svInterface);
        if (thread_data[j].goCbRef) free(thread_data[j].goCbRef);
        if (thread_data[j].scenarioConfigFile) free(thread_data[j].scenarioConfigFile);
        if (thread_data[j].svIDs) free(thread_data[j].svIDs);
    }
    if (thread_data) 
    {
        free(thread_data); 
        thread_data = NULL; // Set to NULL to avoid dangling pointer
     }
    if (threads) { free(threads); threads = NULL; }
    instance_count = 0;
    return FAIL;
}


bool SVPublisher_start(void) {
    if (threads == NULL || thread_data == NULL || instance_count <= 0) {
        LOG_ERROR("SV_Publisher", "SV Publisher not initialized. Call SVPublisher_init first.");
        return FAIL;
    }
    signal(SIGINT, sigint_handler);
    bool all_threads_created = SUCCESS;

    for (int i = 0; i < instance_count; i++) {
        if (pthread_create(&threads[i], NULL, thread_task, &thread_data[i]) != 0) {
            LOG_ERROR("SV_Publisher", "Failed to create thread for instance %d: %s", i, strerror(errno));
            all_threads_created = FAIL;
        } else {
            LOG_INFO("SV_Publisher", "Created thread for instance %d", i);
        }
    }
    printf("All threads created successfully: %d\n", all_threads_created);
    LOG_INFO("SV_Publisher", "SV Publisher threads started.");

    return all_threads_created;
}


void SVPublisher_stop()
{
       LOG_INFO("SV_Publisher", "Signaling SV Publisher threads to shut down...");

    // Set the running flag to false to signal threads to exit their loops
    running = 0; // Assuming 'running' is a global or accessible variable

    // Wait for all threads to finish their execution and cleanup
    if (threads != NULL) {
        for (int i = 0; i < instance_count; i++) {
            if (threads[i] != 0) { // Check if thread was successfully created
                LOG_INFO("SV_Publisher", "Joining thread for instance %d...", i);
                if (pthread_join(threads[i], NULL) != 0) {
                    LOG_ERROR("SV_Publisher", "Failed to join thread for instance %d: %s", i, strerror(errno));
                }
              printf("SV_Publisher  Joined thread for instance %d.\n", i);
            }
        }
        free(threads);
        threads = NULL;
    }
    // Now that all threads have exited and performed their local cleanup,
    // it's safe to free the shared thread_data array.
    if (thread_data != NULL) {
        // The strdup'd strings are now freed by each thread_task, so remove their free calls here.
        // Only free the thread_data array itself.
        free(thread_data);
        thread_data = NULL;
    }

    instance_count = 0;
    printf("SV_Publisher resources cleaned up.");
}


 void setup_timer(ThreadData *data)
{
    struct sigevent sev;
    struct itimerspec ts;
    timer_t timerid;
    int signal_num = SIGRTMIN + (data->parameters.appId % 10); // Unique signal from SIGRTMIN to SIGRTMIN+9

    // Ensure signal number is within valid range
    if (signal_num > SIGRTMAX)
    {
        printf("Signal number %d exceeds SIGRTMAX for appid %u\n", signal_num, data->parameters.appId);
        return;
    }

    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_SIGNAL; // Deliver signal to process, rely on thread-specific data
    sev.sigev_signo = signal_num;
    sev.sigev_value.sival_ptr = data; // Pass ThreadData pointer

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = timer_handler;
    sa.sa_flags = SA_RESTART | SA_SIGINFO; // Use SA_SIGINFO to get sival_ptr
    if (sigaction(signal_num, &sa, NULL) == -1)
    {
        perror("sigaction failed");
        return;
    }

    if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1)
    {
        perror("timer_create failed");
        return;
    }

    ts.it_value.tv_sec = 0;
    ts.it_value.tv_nsec = 416000; // 416 us
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 416000;

    if (timer_settime(timerid, 0, &ts, NULL) == -1)
    {
        perror("timer_settime failed");
        timer_delete(timerid);
        return;
    }

    data->timerid = timerid;
    printf("Timer started for appid %u with signal %d\n", data->parameters.appId, signal_num);
}


