#define _GNU_SOURCE
#include "sv_publisher.h"
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
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
/* choose ANA method or OMT method to generate sinus signal */
// #define SINU_METHOD_ANA

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

typedef struct
{
    uint16_t GOOSEappId; // app id svpub
    char *svInterface;
    const char *scenarioConfigFile;
    char **svIDs;
    int svIDCount;
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
/* Number of loops incremented for each sample sent (tracking time in 256us steps) */
static uint32_t sNbLoop208us = 0;
/* Variables for voltages and currents */
static int channel1_voltage1, channel1_voltage2, channel1_voltage3;
static int channel1_current1, channel1_current2, channel1_current3;

static int tbIndData[COM_VDPA_NB_ECH_PAR_SV][COM_VDPA_NB_DATA_PAR_ECH];

/* Function to calculate sine and cosine values */
extern void fComCalSinCos(float theta, float *pSinVal, float *pCosVal);
/* Function to simulate the STPM values based on input */
static float fOmtStpmSimuGetVal(float veff, float freq, float phi, uint16_t harmoniques);

void sigint_handler(int sig)
{
    if (sig == SIGINT)
    {
        printf("\nReceived Ctrl+C, shutting down...\n");
        running = 0;
    }
}



static int string_to_mac(const char *mac_str, uint8_t *dstAddress)
{
    if (!mac_str)
        return -1;

    char temp[18]; // Buffer for MAC string (17 chars + null terminator)
    strncpy(temp, mac_str, sizeof(temp));
    temp[sizeof(temp) - 1] = '\0';

    char *token = strtok(temp, ":");
    int i = 0;
    while (token && i < 6)
    {
        dstAddress[i++] = (uint8_t)strtol(token, NULL, 16);
        token = strtok(NULL, ":");
    }
    return (i == 6) ? 0 : -1; // Success if exactly 6 bytes parsed
}

// Signal handler for SIGALRM
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

// Setup periodic timer
static void setup_timer(ThreadData *data)
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

static void delay_416us(void)
{
    struct timespec req;
    struct timespec rem;
    // Attendre COM_VDPA_PERIODE_SV_EN_NS
    req.tv_sec = 0;
    req.tv_nsec = COM_VDPA_PERIODE_SV_EN_NS;
    while (0 != nanosleep(&req, &rem))
    {
        req.tv_sec = rem.tv_sec;
        req.tv_nsec = rem.tv_nsec;
    }
}

//==============================================================================
/**
 * @brief Generer une valeur sinusoidale
 */
//==============================================================================
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
    if(data->svIDCount>=2)
    {
        
       data->asdu1 = SVPublisher_addASDU(data->svPublisher, data->svIDs[0], NULL, 1);
       data->asdu2 = SVPublisher_addASDU(data->svPublisher, data->svIDs[1], NULL, 1);
    }
    else
     data->asdu1 = SVPublisher_addASDU(data->svPublisher, data->svIDs[0], NULL, 1);
     data->asdu2 = SVPublisher_addASDU(data->svPublisher, "svpub2", NULL, 1);

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


    printf("svInterface %s\nappid 0x%04x\ndstMac: %02x:%02x:%02x:%02x:%02x:%02x\n",
           data->svInterface, data->parameters.appId,
           data->parameters.dstAddress[0], data->parameters.dstAddress[1], data->parameters.dstAddress[2],
           data->parameters.dstAddress[3], data->parameters.dstAddress[4], data->parameters.dstAddress[5]);


    data->parameters.vlanPriority = 0;
   
    data->svPublisher = SVPublisher_create(&data->parameters, data->svInterface);
    if (!data->svPublisher)
    {
        printf("Failed to create SVPublisher for appid %u\n", data->parameters.appId);
        goto cleanup;
    }
    if (loadScenarioFile(data->scenarioConfigFile) != 0)
    {
        printf("Erreur loading scenario file\n");
        goto cleanup;
    }
    printf("phase_count = %u\n", phase_count);

    // Optional: Comment out GOOSE if not needed, or fix with correct interface
    if (setupGooseSubscriber(data) != 0)
    {
        printf("Failed to setup GOOSE Subscriber\n");
        goto cleanup;
    }

    setupSVPublisher(data);
    if (!data->svPublisher)
    {
        printf("Failed to create SVPublisher\n");
        goto cleanup;
    }


    phase_start_tick = tick_208_us;
    phase_duration_ticks = phases[current_phase].duration_ms * 1000 / (unsigned int)DELAY_208US;
   

    setup_timer(data); // Start periodic publishing
    // Run indefinitely until Ctrl+C
    while (running)
    {
        sleep(1); // Sleep to reduce CPU usage; timer_handler does the publishing
    }

cleanup:
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
    printf("Thread finished publishing\n");
    return NULL;
}

int main(int argc, char **argv)
{
    printf("Starting VDPA SV Publisher with Scenario +parse and threads\n");
    printf("UID: %d\n", getuid());
   
    const char *filename = "/home/eya/Bureau/TEST_SAMPLE_VALUES_IEC618500_PARSER/config2.xml";
    signal(SIGINT, sigint_handler);

    Instance *instances;
    int instance_count, sampling_time;
    if (parse_xml("config2.xml", &instances, &instance_count, &sampling_time) != 0)
    {
        fprintf(stderr, "Failed to parse XML\n");
        return 1;
    }

    // Update timer with sampling_time from XML
    printf("Sampling time: %d us\n", sampling_time);

    pthread_t *threads = malloc(instance_count * sizeof(pthread_t));
    ThreadData *thread_data = malloc(instance_count * sizeof(ThreadData));
    // uint8_t(*mac_addresses)[6] = malloc(instance_count * sizeof(uint8_t[6]));
    // if (!threads || !thread_data || !mac_addresses)
    // {
    //     fprintf(stderr, "Memory allocation failed!\n");
    //     return 1;
    // }

    for (int i = 0; i < instance_count; i++)
    {
        thread_data[i].parameters.vlanPriority = 0;
        thread_data[i].parameters.vlanId = 0;
        thread_data[i].parameters.appId = instances[i].appId; // svappid
        thread_data[i].GOOSEappId = instances[i].gooseAppId;
        thread_data[i].svInterface = strdup(instances[i].svInterface);
        thread_data[i].goCbRef = strdup(instances[i].goCbRef);
        thread_data[i].scenarioConfigFile = instances[i].scenarioConfigFile;
        thread_data[i].svIDCount = instances[i].svIDCount;
        memcpy(thread_data[i].parameters.dstAddress, instances[i].dstMac, 6);

        thread_data[i].svIDs = malloc(instances[i].svIDCount * sizeof(char *));
        if (!thread_data[i].svIDs)
        {
            printf("Failed to allocate svIDs for thread %d\n", i + 1);
            exit(1);
        }
        for (int j = 0; j < instances[i].svIDCount; j++)
        {
            if (instances[i].svIDs[j])
            {
                thread_data[i].svIDs[j] = strdup(instances[i].svIDs[j]);
               // printf("Copied svID[%d] for thread %d: %s\n", j, i + 1, thread_data[i].svIDs[j]);
            }
            else
            {
                printf("Warning: instances[%d].svIDs[%d] is NULL\n", i, j);
                thread_data[i].svIDs[j] = strdup("default_svID"); // Fallback
            }
        }


        if (pthread_create(&threads[i], NULL, thread_task, &thread_data[i]) != 0)
        {
            perror("Failed to create thread");
            goto cleanup;
        }
        printf("Created thread %d\n", i + 1);
    }

    for (int i = 0; i < instance_count; i++)
    {
        pthread_join(threads[i], NULL);
    }

cleanup:
    for (int i = 0; i < instance_count; i++)
    {
        free(thread_data[i].svInterface);
        free(thread_data[i].scenarioConfigFile);
        free(thread_data[i].svIDs); // Free allocated appId
    }
    free_instances(instances, instance_count);
    xmlCleanupParser();
    free(threads);
    free(thread_data);
    // free(mac_addresses);
    printf("All %d threads completed!\n", instance_count);
    munlockall();
    return 0;
}