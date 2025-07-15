#define _GNU_SOURCE
#include "sv_publisher.h"
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "iec61850_server.h"
#include "hal_thread.h"
#include "goose_receiver.h"
#include "goose_subscriber.h"
#include "mms_value.h"  // For MmsValue_printToBuffer if needed
#include <sched.h>
#include <pthread.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/time.h>

/* choose ANA method or OMT method to generate sinus signal */
//#define SINU_METHOD_ANA
#define MAX_GOOSE_SUBSCRIPTIONS 24

#define COM_VDPA_NB_ECH_PAR_SV         2
#define FREQ_EN_HZ         (f32)50.
#define PI                 (f32)3.1415926536
#define MAX_PHASES 150
/* 2400 for 2 samples and 4800 for 1 sample*/
#define SAMPLES_PER_SECOND 2400

/* Sampling time defined as 208 microseconds */
#define SAMPLING_TIME_208US (float)0.000208f
#define DELAY_416US (float)(SAMPLING_TIME_208US*2.f)
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
#define COM_VDPA_PERIODE_SV_EN_NS      416667   // 2*208.33 us (Freq 4800Hz -> 1 ech toute les 208.33 us)
#define COM_VDPA_CADENCE_ECH_EN_US     (f32)(COM_VDPA_PERIODE_SV_EN_NS/COM_VDPA_NB_ECH_PAR_SV/1000.)

typedef float f32;
typedef f32 float32_t;    // Pour compatibilite ancienne version

typedef struct {
    char path[128];
    uint8_t mac[6];
    uint16_t appId;
} GooseConfig;

typedef struct {
    char path[128];
    uint8_t mac[6];
    uint16_t appId;
    uint32_t lastStNum;
    uint64_t faultStartTimeNs;
    bool isMeasuring;
    bool latencyMeasured;
} GooseSubscriptionState;

GooseSubscriptionState gooseStates[MAX_GOOSE_SUBSCRIPTIONS];


GooseConfig gooseConfigs[MAX_GOOSE_SUBSCRIPTIONS];
int gooseCount = 0;

CommParameters parameters={0,0,0x5000,{0x01, 0x0C, 0xCD, 0x01, 0x00, 0x01}};
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


typedef struct {
    float channel1_voltage[3]; // amplitudes
    float channel1_voltage_phi[3]; // phase shifts in degrees
    float channel1_current[3]; // amplitudes
    float channel1_current_phi[3]; // phase shifts in degrees
    int duration_ms;
} PhaseSettings;

/* GOOSE Subscriber Code Integration */
static GooseReceiver gooseReceiver = NULL;
static GooseSubscriber gooseSubscriber = NULL;
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
static f32 pasCrs = FREQ_EN_HZ*(f32)360.*COM_VDPA_CADENCE_ECH_EN_US/(f32)1000000.;

PhaseSettings phases[MAX_PHASES];
int phase_count = 0;

uint64_t tick_208_us = 0;
volatile static int running = 1;
/* Number of loops incremented for each sample sent (tracking time in 256us steps) */
static uint32_t sNbLoop208us = 0;
/* Variables for voltages and currents */
static int channel1_voltage1, channel1_voltage2, channel1_voltage3;
static int channel1_current1, channel1_current2, channel1_current3;

static int tbIndData[COM_VDPA_NB_ECH_PAR_SV][COM_VDPA_NB_DATA_PAR_ECH];

static SVPublisher svPublisher;
static SVPublisher_ASDU asdu1;
static SVPublisher_ASDU asdu2;
static SVPublisher_ASDU asdu;

/* Function to calculate sine and cosine values */
extern void fComCalSinCos(float theta, float *pSinVal, float *pCosVal);
/* Function to simulate the STPM values based on input */
static float fOmtStpmSimuGetVal(float veff, float freq, float phi, uint16_t harmoniques);

void sigint_handler(int signalId) 
{
    running = 0;
}



int loadGooseConfig(const char* filename) 
{
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Could not open GOOSE config file");
        return -1;
    }

    char line[256];

    while (fgets(line, sizeof(line), file) && gooseCount < MAX_GOOSE_SUBSCRIPTIONS) 
    {
        if (strstr(line, "GOOSE_PATH=")) 
        {
            sscanf(line, "GOOSE_PATH=%s MAC=%hhx:%hhx:%hhx:%hhx:%hhx:%hhx APPID=0x%hx",
                   gooseConfigs[gooseCount].path,
                   &gooseConfigs[gooseCount].mac[0], &gooseConfigs[gooseCount].mac[1],
                   &gooseConfigs[gooseCount].mac[2], &gooseConfigs[gooseCount].mac[3],
                   &gooseConfigs[gooseCount].mac[4], &gooseConfigs[gooseCount].mac[5],
                   &gooseConfigs[gooseCount].appId);

            gooseCount++;
        }
    }

    fclose(file);
    return 0;
}

static void gooseListener(GooseSubscriber subscriber, void* parameter)
{
    GooseSubscriptionState* state = (GooseSubscriptionState*)parameter;
    uint32_t newStNum = GooseSubscriber_getStNum(subscriber);

    /* Only process if stNum changed */
    if (newStNum != state->lastStNum) 
    {
        state->lastStNum = newStNum;

        /* If we are currently measuring and haven't recorded latency yet */
        if (state->isMeasuring && !state->latencyMeasured) 
        {
            uint64_t now = Hal_getTimeInNs();
            uint64_t latency = now - state->faultStartTimeNs;

            printf("GOOSE [%s]: Latency measured: %f ms (stNum changed to %u)\n",
                   state->path, ((float)latency / 1000000.f), newStNum);

            state->latencyMeasured = true;
        }
    }
}

static int setupGooseSubscribers(const char* interfaceId) 
{
    gooseReceiver = GooseReceiver_create();
    if (gooseReceiver == NULL) 
    {
        printf("Failed to create GooseReceiver\n");
        return -1;
    }

    GooseReceiver_setInterfaceId(gooseReceiver, interfaceId);

    for (int i = 0; i < gooseCount; i++) 
    {
        GooseSubscriber* subscriber = GooseSubscriber_create(gooseConfigs[i].path, NULL);
        if (subscriber == NULL) 
        {
            printf("Failed to create GooseSubscriber for path %s\n", gooseConfigs[i].path);
            continue;
        }

        GooseSubscriber_setDstMac(subscriber, gooseConfigs[i].mac);
        GooseSubscriber_setAppId(subscriber, gooseConfigs[i].appId);

        /* Initialize state tracking for this GOOSE */
        strcpy(gooseStates[i].path, gooseConfigs[i].path);
        memcpy(gooseStates[i].mac, gooseConfigs[i].mac, 6);
        gooseStates[i].appId = gooseConfigs[i].appId;
        gooseStates[i].lastStNum = 0;
        gooseStates[i].faultStartTimeNs = 0;
        gooseStates[i].isMeasuring = false;
        gooseStates[i].latencyMeasured = false;

        /* Assign the listener with a unique state pointer */
        GooseSubscriber_setListener(subscriber, gooseListener, &gooseStates[i]);

        GooseReceiver_addSubscriber(gooseReceiver, subscriber);

        printf("GOOSE Subscriber added: Path=%s, MAC=%02x:%02x:%02x:%02x:%02x:%02x, AppID=0x%X\n",
               gooseConfigs[i].path,
               gooseConfigs[i].mac[0], gooseConfigs[i].mac[1], gooseConfigs[i].mac[2],
               gooseConfigs[i].mac[3], gooseConfigs[i].mac[4], gooseConfigs[i].mac[5],
               gooseConfigs[i].appId);
    }

    GooseReceiver_start(gooseReceiver);

    if (!GooseReceiver_isRunning(gooseReceiver)) 
    {
        printf("Failed to start GOOSE subscribers\n");
        GooseReceiver_destroy(gooseReceiver);
        return -1;
    }

    return 0;
}

// Signal handler for SIGALRM
void timer_handler(int signum) 
{
    bool faultCondition=false;
    Quality q = QUALITY_VALIDITY_GOOD;


    if (running) 
    {
        for (int sample = 0; sample < COM_VDPA_NB_ECH_PAR_SV; sample++) 
        {
            if(0 ==sample)
            {
                asdu = asdu1;
            }
            else
            {
                asdu = asdu2;
            }
#ifdef SINU_METHOD_ANA
            /* Calculate instantaneous values and send samples */
            channel1_voltage1 = (int)(100.f * fComStpmSimuGetVal(phases[current_phase].channel1_voltage[0], 0.0f)  );   // V1
            channel1_voltage2 = (int)(100.f * fComStpmSimuGetVal(phases[current_phase].channel1_voltage[1], 240.0f)); // V2
            channel1_voltage3 = (int)(100.f * fComStpmSimuGetVal(phases[current_phase].channel1_voltage[2], 120.0f)); // V3

            channel1_current1 = (int)(1000.f * fComStpmSimuGetVal(phases[current_phase].channel1_current[0], 0.0f)  );   // I1
            channel1_current2 = (int)(1000.f * fComStpmSimuGetVal(phases[current_phase].channel1_current[1], 240.0f)); // I2
            channel1_current3 = (int)(1000.f * fComStpmSimuGetVal(phases[current_phase].channel1_current[2], 120.0f)); // I3
#else
            channel1_voltage1 = (int)(100.f * fOmtStpmSimuGetVal(phases[current_phase].channel1_voltage[0], FREQ_EN_HZ, phases[current_phase].channel1_voltage_phi[0],0));   // V1
            channel1_voltage2 = (int)(100.f * fOmtStpmSimuGetVal(phases[current_phase].channel1_voltage[1], FREQ_EN_HZ, phases[current_phase].channel1_voltage_phi[1],0)); // V2
            channel1_voltage3 = (int)(100.f * fOmtStpmSimuGetVal(phases[current_phase].channel1_voltage[2], FREQ_EN_HZ, phases[current_phase].channel1_voltage_phi[2],0)); // V3

            channel1_current1 = (int)(1000.f * fOmtStpmSimuGetVal(phases[current_phase].channel1_current[0],FREQ_EN_HZ, phases[current_phase].channel1_current_phi[0],0)  );   // I1
            channel1_current2 = (int)(1000.f * fOmtStpmSimuGetVal(phases[current_phase].channel1_current[1],FREQ_EN_HZ, phases[current_phase].channel1_current_phi[1],0)); // I2
            channel1_current3 = (int)(1000.f * fOmtStpmSimuGetVal(phases[current_phase].channel1_current[2],FREQ_EN_HZ, phases[current_phase].channel1_current_phi[2],0)); // I3
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

            SVPublisher_ASDU_setINT32(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_I4], channel1_current1+channel1_current2+channel1_current3);
            SVPublisher_ASDU_setQuality(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_I4Q], q);

            SVPublisher_ASDU_setINT32(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_V1], channel1_voltage1);
            SVPublisher_ASDU_setQuality(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_V1Q], q);

            SVPublisher_ASDU_setINT32(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_V2], channel1_voltage2);
            SVPublisher_ASDU_setQuality(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_V2Q], q);

            SVPublisher_ASDU_setINT32(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_V3], channel1_voltage3);
            SVPublisher_ASDU_setQuality(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_V3Q], q);

            SVPublisher_ASDU_setINT32(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_V4], channel1_voltage1+channel1_voltage2+channel1_voltage3);
            SVPublisher_ASDU_setQuality(asdu, tbIndData[sample][COM_VDPA_ECH_DATA_IND_V4Q], q);

            //printf("Channel 1 - Voltage1: %d, Voltage2: %d, Voltage3: %d\n", channel1_voltage1, channel1_voltage2, channel1_voltage3);
            //printf("Channel 1 - Current1: %d, Current2: %d, Current3: %d\n", channel1_current1, channel1_current2, channel1_current3);
            
#ifdef SINU_METHOD_ANA               
            angleCrs += pasCrs;
            if (angleCrs > (f32)360.)
            {
                angleCrs -= (f32)360.;
            }
#endif
            /* Latency measurement logic */
            /* Detect when current > 1 and start measurement if not started yet */
            faultCondition = (phases[current_phase].channel1_current[0] > 400.0f || phases[current_phase].channel1_current[1] > 400.0f || phases[current_phase].channel1_current[2] > 400.0f);

            /* If the current returns to exactly 1.0, reset measuring state */
            bool resetCondition =
                (phases[current_phase].channel1_current[0] == 400.0f) &&
                    (phases[current_phase].channel1_current[1] == 400.0f) &&
                    (phases[current_phase].channel1_current[2] == 400.0f);

            if (resetCondition) 
            {
                for (int i = 0; i < gooseCount; i++) 
                {
                    gooseStates[i].isMeasuring = false;
                    gooseStates[i].latencyMeasured = false;
                }
            }

            tick_208_us++;
            if ((current_phase < phase_count) && (end_test == 0)) 
            {
                if ((tick_208_us - phase_start_tick) >= phase_duration_ticks) 
                {
                    //printf("[time %u ms] Passing Phase from %d -> %d \n\r", phases[current_phase].duration_ms, current_phase, current_phase + 1);
                    current_phase++;
                    phase_duration_ticks = phases[current_phase].duration_ms * 1000 / (unsigned int)DELAY_208US;
                    phase_start_tick = tick_208_us;
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
        }

        /* Publish updated SV data */
        SVPublisher_publish(svPublisher);

      /* Start latency measurement when fault occurs */
        if (faultCondition) 
        {
            for (int i = 0; i < gooseCount; i++) 
            {
                if (!gooseStates[i].isMeasuring) 
                {
                    gooseStates[i].isMeasuring = true;
                    gooseStates[i].latencyMeasured = false;
                    gooseStates[i].faultStartTimeNs = Hal_getTimeInNs();
                    printf("GOOSE [%s]: Fault detected, start measuring latency...\n", gooseStates[i].path);
                }
            }
        }
    }
}

// Setup periodic timer
static void setup_timer() 
{
    struct sigaction sa;
    struct itimerval timer;

    // Configure the signal handler
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &timer_handler;
    sigaction(SIGALRM, &sa, NULL);

    // Configure the timer to expire every 416 microseconds
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 416; // Initial delay
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 416; // Repeating interval

    setitimer(ITIMER_REAL, &timer, NULL);
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

static void setup_realtime_and_affinity(void) 
{
    /* Lock memory to prevent paging delays */
    if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0) {
        perror("mlockall failed");
        exit(EXIT_FAILURE);
    }

    /* Set CPU affinity to CPU1 */
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset); // CPU1
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) < 0) {
        perror("sched_setaffinity");
        exit(EXIT_FAILURE);
    }

    /* Set Real-Time Priority and Scheduling Policy */
    struct sched_param param;
    memset(&param, 0, sizeof(param));
    param.sched_priority = 90; // High RT priority (1-99)
    if (sched_setscheduler(0, SCHED_FIFO, &param) < 0) {
        perror("sched_setscheduler");
        exit(EXIT_FAILURE);
    }

    /* Optional: reduce timer resolution for better accuracy */
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1000000; // 1ms timer resolution
    if (clock_settime(CLOCK_MONOTONIC, &ts) < 0) {
        // Not all systems allow changing CLOCK_MONOTONIC, ignore if fails
    }

    /* Pre-fault dynamic memory if possible to avoid page faults at runtime */
    // Touch all pages used by the program now so they won't fault later.
    // For large buffers, consider using memset on them here.

    printf("Real-time settings applied: CPU affinity set to CPU1, SCHED_FIFO priority=90\n");
}
/**
* @brief Generer une valeur sinusoidale
*/
//==============================================================================
static f32 fComStpmSimuGetVal(f32 veff, f32 phi)
{
   f32 theta;

   theta = angleCrs + phi;          // Angle en degre
   if (theta > (f32)360.)
   {
      theta -= (f32)360.;
   }
   theta = theta*2.*PI/(f32)360.;   // Angle en radians
   return(veff*1.41421356*sin(theta));
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
        if (theta > 360.0f) {
            theta -= 360.0f;
        }
        fComCalSinCos(theta, &fsin, &fcos);
    } 
    else 
    {
        float fsinh;
        fsin = 0.0f;
        for (uint8_t ind_rang = 0U; ind_rang < 16; ind_rang++) {
            if (0U != ((1 << ind_rang) & harmoniques)) {
                theta = ((freq * (ind_rang + 1)) / 1000000.0f) * (float)sNbLoop208us * DELAY_208US;
                theta -= (uint32_t)theta;
                theta *= 360.0f;
                theta += phi;
                if (theta > 360.0f) {
                    theta -= 360.0f;
                }
                fComCalSinCos(theta, &fsinh, &fcos);
                fsin += fsinh;
            }
        }
    }
    return (veff * 1.41421356f * fsin);
}

static void setupSVPublisher(const char* svInterface) 
{


    svPublisher = SVPublisher_create(&parameters, svInterface);

    if (svPublisher)
    {
       
        asdu1 = SVPublisher_addASDU(svPublisher, "svpub1", NULL, 1);
        asdu2 = SVPublisher_addASDU(svPublisher, "svpub2", NULL, 1);

        for (uint8_t no_ech = 0U; no_ech < COM_VDPA_NB_ECH_PAR_SV; no_ech++) 
        {
            if(0 == no_ech)
            {
                asdu = asdu1;
            }
            else
            {
                asdu = asdu2;
            }
            
            for (uint8_t no_data = 0U; no_data < COM_VDPA_NB_DATA_PAR_ECH; no_data++)
            {
                if((no_data & 0x01) == 0)
                {
                    tbIndData[no_ech][no_data] = SVPublisher_ASDU_addINT32(asdu);
                }
                else
                {
                    tbIndData[no_ech][no_data] = SVPublisher_ASDU_addQuality(asdu);
                }

            }

            SVPublisher_ASDU_setSmpCntWrap(asdu, SAMPLES_PER_SECOND);
            SVPublisher_ASDU_setRefrTm(asdu, 0);

        }
        SVPublisher_setupComplete(svPublisher);

    }
}

int loadScenarioFile(const char* filename) 
{
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Could not open scenario file");
        return -1;
    }

    for (int i = 0; i < MAX_PHASES; i++) {
        for (int j = 0; j < 3; j++) {
            phases[i].channel1_voltage[j] = 0.0f;
            phases[i].channel1_voltage_phi[j] = 0.0f; // initialize phi
            phases[i].channel1_current[j] = 0.0f;
            phases[i].channel1_current_phi[j] = 0.0f; // initialize phi
        }
        phases[i].duration_ms = 0;
    }

    char line[256];
    int current_phase = -1;
    float value;
    int duration;

    while (fgets(line, sizeof(line), file) && current_phase < MAX_PHASES - 1) 
    {
        if (strstr(line, "# Phase")) {
            current_phase++;
        } 
        else if (current_phase >= 0) 
        {
            if (sscanf(line, "duration_ms=%d", &duration) == 1) {
                phases[current_phase].duration_ms = duration;
            }
            else if (sscanf(line, "channel1_voltage1=%f", &value) == 1) {
                phases[current_phase].channel1_voltage[0] = value;
            }
            else if (sscanf(line, "channel1_voltage2=%f", &value) == 1) {
                phases[current_phase].channel1_voltage[1] = value;
            }
            else if (sscanf(line, "channel1_voltage3=%f", &value) == 1) {
                phases[current_phase].channel1_voltage[2] = value;
            }
            else if (sscanf(line, "channel1_current1=%f", &value) == 1) {
                phases[current_phase].channel1_current[0] = value;
            }
            else if (sscanf(line, "channel1_current2=%f", &value) == 1) {
                phases[current_phase].channel1_current[1] = value;
            }
            else if (sscanf(line, "channel1_current3=%f", &value) == 1) {
                phases[current_phase].channel1_current[2] = value;
            }
            else if (sscanf(line, "channel1_voltage1_phi=%f", &value) == 1) {
                phases[current_phase].channel1_voltage_phi[0] = value;
            }
            else if (sscanf(line, "channel1_voltage2_phi=%f", &value) == 1) {
                phases[current_phase].channel1_voltage_phi[1] = value;
            }
            else if (sscanf(line, "channel1_voltage3_phi=%f", &value) == 1) {
                phases[current_phase].channel1_voltage_phi[2] = value;
            }
            else if (sscanf(line, "channel1_current1_phi=%f", &value) == 1) {
                phases[current_phase].channel1_current_phi[0] = value;
            }
            else if (sscanf(line, "channel1_current2_phi=%f", &value) == 1) {
                phases[current_phase].channel1_current_phi[1] = value;
            }
            else if (sscanf(line, "channel1_current3_phi=%f", &value) == 1) {
                phases[current_phase].channel1_current_phi[2] = value;
            }
        }
    }

    phase_count = current_phase + 1;
    fclose(file);

    return 0;
}

static int parseMacAddress(const char* macStr, uint8_t* macBytes) {
    return sscanf(macStr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                  &macBytes[0], &macBytes[1], &macBytes[2],
                  &macBytes[3], &macBytes[4], &macBytes[5]) == 6 ? 0 : -1;
}


int main(int argc, char** argv) 
{
    char* svInterface;
    char* scenarioConfigFile;
    char* gooseConfigFile;
    char* svDestMacStr;


    if (argc < 6) 
    {
        fprintf(stderr, "Usage: %s <interface> <scenarioFile> <appIdHex> <svDestMac> <gooseConfigFile>\n", argv[0]);
        return -1;
    }
    setup_realtime_and_affinity();

    svInterface = argv[1];
    scenarioConfigFile = argv[2];

    // Parse AppID from argument 3
    parameters.appId = (uint16_t)strtoul(argv[3], NULL, 16);
    // Parse destination MAC address from argument 4
    svDestMacStr = argv[4];
    if (parseMacAddress(svDestMacStr, parameters.dstAddress) != 0) {
        fprintf(stderr, "Invalid SV MAC address: %s\n", svDestMacStr);
        return -1;
    }

    // GOOSE config file from argument 5
    gooseConfigFile = argv[5];
    
    printf("svInterface: %s, Scenario File: %s, AppID: 0x%X, SV MAC: %s, GOOSE Config File: %s\n",
           svInterface, scenarioConfigFile, parameters.appId, svDestMacStr, gooseConfigFile);

    if (loadScenarioFile(scenarioConfigFile) != 0)
    {
        printf("Erreur loading scenario file \n\r");
        return -1;
    }
    if (loadGooseConfig(gooseConfigFile) != 0) 
    {
        printf("Error loading GOOSE configuration file.\n");
        return -1;
    }
    printf("phase_count = %u \n\r",phase_count);

    signal(SIGINT, sigint_handler);


     /* Setup GOOSE Subscriber */
    if (setupGooseSubscribers(svInterface) != 0) 
    {
        printf("Failed to setup GOOSE Subscriber\n");
        return -1;
    }

    setupSVPublisher(svInterface);

    if (svPublisher) 
    {
        phase_start_tick = tick_208_us;
        phase_duration_ticks = phases[current_phase].duration_ms * 1000 / (unsigned int)DELAY_208US;
        setup_timer(); // Start the periodic timer

        // Wait until interrupted
        while (running) {
            pause();
        }
    }
    SVPublisher_destroy(svPublisher);
    // Stop and destroy GOOSE receiver
    if (gooseReceiver) 
    {
        GooseReceiver_stop(gooseReceiver);
        GooseReceiver_destroy(gooseReceiver);
    }
    munlockall();
    return 0;
}

