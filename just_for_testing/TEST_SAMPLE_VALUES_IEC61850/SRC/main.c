#include "sv_publisher.h"
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>


#define MAX_PHASES 150

typedef struct {
    float channel1_voltage[3];
    float channel1_current[3];
    float channel2_voltage[3];
    float channel2_current[3];
    int duration_ms;
} PhaseSettings;

PhaseSettings phases[MAX_PHASES];
int phase_count = 0;

uint64_t tick_256_us = 0;
volatile static int running = 0;
void delay_256us(void) 
{
    struct timespec start, current;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (1) 
    {
        clock_gettime(CLOCK_MONOTONIC, &current);
        // Check if 256 microseconds (0.000256 seconds) have passed
        if ((current.tv_sec - start.tv_sec) + (current.tv_nsec - start.tv_nsec) / 1E9 >= 0.000256) 
        {
            break;
        }
    }
}
void sigint_handler(int signalId)
{
    running = 0;
}
/* Constants for RMS values */
#define OMT_VEFF_RMS_VOLTAGE (float)8.0f
#define OMT_VEFF_RMS_CURRENT (float)0.0f

#define OMT_STPM_VAL_RMS_1V (float)1.0f
#define OMT_STPM_VAL_RMS_1A (float)1.0f


//#define OMT_STPM_VAL_RMS_1V (float)139972.0f
//#define OMT_STPM_VAL_RMS_1A (float)753908.0f
/* Sampling time defined as 256 microseconds */
#define SAMPLING_TIME_256US (float)0.000256f

/* Signal frequency (for example 50 Hz) */
#define SIGNAL_FREQUENCY (float)50.0f

/* Number of loops for one cycle based on the sample time and frequency */
#define LOOPS_PER_CYCLE (uint32_t)((1.0f / SIGNAL_FREQUENCY) / SAMPLING_TIME_256US)

/* Number of samples per second (3906 samples at 256us per sample) */
#define SAMPLES_PER_SECOND 3906

/* Variables for voltages and currents (3 per channel) */
static float channel1_voltage1, channel1_voltage2, channel1_voltage3;
static float channel1_current1, channel1_current2, channel1_current3;
static float channel2_voltage1, channel2_voltage2, channel2_voltage3;
static float channel2_current1, channel2_current2, channel2_current3;

/* Number of loops incremented for each sample sent (tracking time in 256us steps) */
static uint32_t sNbLoop256us = 0;

static int vol1, vol2, vol3, vol4, vol5, vol6;
static int amp1, amp2, amp3, amp4, amp5, amp6;
static int vol1q, vol2q, vol3q, vol4q, vol5q, vol6q;
static int amp1q, amp2q, amp3q, amp4q, amp5q, amp6q;

static SVPublisher svPublisher;
static SVPublisher_ASDU asdu;

/* Function to calculate sine and cosine values */
extern void fComCalSinCos(float theta, float *pSinVal, float *pCosVal);

/* Function to simulate the STPM values based on input */
static float fOmtStpmSimuGetVal(float veff, float freq, float phi, uint16_t harmoniques)
{
    float fsin;
    float fcos;
    float theta;

    if (0U == harmoniques)
    {
        theta = (freq / 1000000.0f) * (float)sNbLoop256us * 256.0f;
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
                theta = ((freq * (ind_rang + 1)) / 1000000.0f) * (float)sNbLoop256us * 256.0f;
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

static void
setupSVPublisher(const char* svInterface)
{
    svPublisher = SVPublisher_create(NULL, svInterface);

    if (svPublisher) 
    {
        asdu = SVPublisher_addASDU(svPublisher, "xxxxMUnn01", NULL, 1);
        /* Initialize voltage and current values for SVPublisher */
 /* Corrected initialization for voltages, currents, and their quality indicators */
        
        
        vol1 = SVPublisher_ASDU_addFLOAT(asdu); // Channel 1 Voltage1
        vol1q = SVPublisher_ASDU_addQuality(asdu);   
        amp1 = SVPublisher_ASDU_addFLOAT(asdu); // Channel 1 Current1
        amp1q = SVPublisher_ASDU_addQuality(asdu);
        
        
        vol2 = SVPublisher_ASDU_addFLOAT(asdu); // Channel 1 Voltage2
        vol2q = SVPublisher_ASDU_addQuality(asdu);
        amp2 = SVPublisher_ASDU_addFLOAT(asdu); // Channel 1 Current2
        amp2q = SVPublisher_ASDU_addQuality(asdu);
        
        vol3 = SVPublisher_ASDU_addFLOAT(asdu); // Channel 1 Voltage3
        vol3q = SVPublisher_ASDU_addQuality(asdu);       
        amp3 = SVPublisher_ASDU_addFLOAT(asdu); // Channel 1 Current3
        amp3q = SVPublisher_ASDU_addQuality(asdu);

        
        vol4 = SVPublisher_ASDU_addFLOAT(asdu); // Channel 2 Voltage1
        vol4q = SVPublisher_ASDU_addQuality(asdu);
        amp4 = SVPublisher_ASDU_addFLOAT(asdu); // Channel 2 Current1
        amp4q = SVPublisher_ASDU_addQuality(asdu);
        

        vol5 = SVPublisher_ASDU_addFLOAT(asdu); // Channel 2 Voltage2
        vol5q = SVPublisher_ASDU_addQuality(asdu);
        amp5 = SVPublisher_ASDU_addFLOAT(asdu); // Channel 2 Current2
        amp5q = SVPublisher_ASDU_addQuality(asdu);
        
  
        vol6 = SVPublisher_ASDU_addFLOAT(asdu); // Channel 2 Voltage3
        vol6q = SVPublisher_ASDU_addQuality(asdu);      
        amp6 = SVPublisher_ASDU_addFLOAT(asdu); // Channel 2 Current3
        amp6q = SVPublisher_ASDU_addQuality(asdu);


        SVPublisher_ASDU_setSmpCntWrap(asdu, SAMPLES_PER_SECOND);  // Set sample count wrap to 3906 samples per second
        SVPublisher_ASDU_setRefrTm(asdu, 0);

        SVPublisher_setupComplete(svPublisher);
    }
}


int loadScenarioFile(const char* filename) 
{
    FILE* file = fopen(filename, "r");
    if (!file) 
    {
        perror("Could not open scenario file");
        return -1;
    }

    // Initialize all phase settings to zero by default
    for (int i = 0; i < MAX_PHASES; i++) 
    {
        for (int j = 0; j < 3; j++) {
            phases[i].channel1_voltage[j] = 0.0f;
            phases[i].channel1_current[j] = 0.0f;
            phases[i].channel2_voltage[j] = 0.0f;
            phases[i].channel2_current[j] = 0.0f;
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

            if (sscanf(line, "duration_ms=%d", &duration) == 1) {
                phases[current_phase].duration_ms = duration;
            }
            if (sscanf(line, "channel1_voltage1=%f", &value) == 1) {
                phases[current_phase].channel1_voltage[0] = value;
            }
            if (sscanf(line, "channel1_voltage2=%f", &value) == 1) {
                phases[current_phase].channel1_voltage[1] = value;
            }
            if (sscanf(line, "channel1_voltage3=%f", &value) == 1) {
                phases[current_phase].channel1_voltage[2] = value;
            }
            if (sscanf(line, "channel1_current1=%f", &value) == 1) {
                phases[current_phase].channel1_current[0] = value;
            }
            if (sscanf(line, "channel1_current2=%f", &value) == 1) {
                phases[current_phase].channel1_current[1] = value;
            }
            if (sscanf(line, "channel1_current3=%f", &value) == 1) {
                phases[current_phase].channel1_current[2] = value;
            }
            if (sscanf(line, "channel2_voltage1=%f", &value) == 1) {
                phases[current_phase].channel2_voltage[0] = value;
            }
            if (sscanf(line, "channel2_voltage2=%f", &value) == 1) {
                phases[current_phase].channel2_voltage[1] = value;
            }
            if (sscanf(line, "channel2_voltage3=%f", &value) == 1) {
                phases[current_phase].channel2_voltage[2] = value;
            }
            if (sscanf(line, "channel2_current1=%f", &value) == 1) {
                phases[current_phase].channel2_current[0] = value;
            }
            if (sscanf(line, "channel2_current2=%f", &value) == 1) {
                phases[current_phase].channel2_current[1] = value;
            }
            if (sscanf(line, "channel2_current3=%f", &value) == 1) {
                phases[current_phase].channel2_current[2] = value;
            }
        }
    }

    phase_count = current_phase + 1;
    printf("phase_count = %d\n\r",phase_count);
    fclose(file);

    return 0;
}

int
main(int argc, char** argv)
{
    char* svInterface;
    char *scenarioConfigFile; // Variable for the scenario config file

    uint64_t current_time=0;
    uint8_t end_test=0;
    float veff_channel1_voltage1=0.0;
    float veff_channel1_voltage2=0.0;
    float veff_channel1_voltage3=0.0;
    float veff_channel1_current1=0.0;
    float veff_channel1_current2=0.0;
    float veff_channel1_current3=0.0;
    float veff_channel2_voltage1=0.0;
    float veff_channel2_voltage2=0.0;
    float veff_channel2_voltage3=0.0;
    float veff_channel2_current1=0.0;
    float veff_channel2_current2=0.0;
    float veff_channel2_current3=0.0;
    if (argc > 1)
        svInterface = argv[1];
    else
        svInterface = "eth0";

    running = 1;

    // Check the second argument for the scenario config file
    if (argc > 2) 
    {
        scenarioConfigFile = argv[2];
    } 
    else 
    {
        return -1;
    }
    if(loadScenarioFile(scenarioConfigFile) != 0)
    {
        return -1;
    }
    signal(SIGINT, sigint_handler);

    setupSVPublisher(svInterface);

    if (svPublisher) 
    {
        Quality q = QUALITY_VALIDITY_GOOD;
        int sampleCount = 0;


        int current_phase = 0;
        uint64_t phase_start_tick = tick_256_us;
        uint64_t phase_duration_ticks = phases[current_phase].duration_ms * 1000 / 256;

        while (running) 
        {
            /* Set veff values for each channel and phase */
            veff_channel1_voltage1 = phases[current_phase].channel1_voltage[0] * OMT_STPM_VAL_RMS_1V;
            veff_channel1_voltage2 = phases[current_phase].channel1_voltage[1] * OMT_STPM_VAL_RMS_1V;
            veff_channel1_voltage3 = phases[current_phase].channel1_voltage[2] * OMT_STPM_VAL_RMS_1V;
            veff_channel1_current1 = phases[current_phase].channel1_current[0] * OMT_STPM_VAL_RMS_1A;
            veff_channel1_current2 = phases[current_phase].channel1_current[1] * OMT_STPM_VAL_RMS_1A;
            veff_channel1_current3 = phases[current_phase].channel1_current[2] * OMT_STPM_VAL_RMS_1A;

            veff_channel2_voltage1 = phases[current_phase].channel2_voltage[0] * OMT_STPM_VAL_RMS_1V;
            veff_channel2_voltage2 = phases[current_phase].channel2_voltage[1] * OMT_STPM_VAL_RMS_1V;
            veff_channel2_voltage3 = phases[current_phase].channel2_voltage[2] * OMT_STPM_VAL_RMS_1V;
            veff_channel2_current1 = phases[current_phase].channel2_current[0] * OMT_STPM_VAL_RMS_1A;
            veff_channel2_current2 = phases[current_phase].channel2_current[1] * OMT_STPM_VAL_RMS_1A;
            veff_channel2_current3 = phases[current_phase].channel2_current[2] * OMT_STPM_VAL_RMS_1A;

            /* Calculate instantaneous values for each channel and phase */
            channel1_voltage1 = fOmtStpmSimuGetVal(veff_channel1_voltage1, SIGNAL_FREQUENCY, 0.0f, 0);
            channel1_voltage2 = fOmtStpmSimuGetVal(veff_channel1_voltage2, SIGNAL_FREQUENCY, 240.0f, 0);
            channel1_voltage3 = fOmtStpmSimuGetVal(veff_channel1_voltage3, SIGNAL_FREQUENCY, 120.0f, 0);
            channel1_current1 = fOmtStpmSimuGetVal(veff_channel1_current1, SIGNAL_FREQUENCY, 0.0f, 0);
            channel1_current2 = fOmtStpmSimuGetVal(veff_channel1_current2, SIGNAL_FREQUENCY, 240.0f, 0);
            channel1_current3 = fOmtStpmSimuGetVal(veff_channel1_current3, SIGNAL_FREQUENCY, 120.0f, 0);

            channel2_voltage1 = fOmtStpmSimuGetVal(veff_channel2_voltage1, SIGNAL_FREQUENCY, 0.0f, 0);
            channel2_voltage2 = fOmtStpmSimuGetVal(veff_channel2_voltage2, SIGNAL_FREQUENCY, 240.0f, 0);
            channel2_voltage3 = fOmtStpmSimuGetVal(veff_channel2_voltage3, SIGNAL_FREQUENCY, 120.0f, 0);
            channel2_current1 = fOmtStpmSimuGetVal(veff_channel2_current1, SIGNAL_FREQUENCY, 0.0f, 0);
            channel2_current2 = fOmtStpmSimuGetVal(veff_channel2_current2, SIGNAL_FREQUENCY, 240.0f, 0);
            channel2_current3 = fOmtStpmSimuGetVal(veff_channel2_current3, SIGNAL_FREQUENCY, 120.0f, 0);

           // printf("Channel 2 - Voltage1: %f, Voltage2: %f, Voltage3: %f\n", channel2_voltage1, channel2_voltage2, channel2_voltage3);
            //printf("Channel 2 - Current1: %f, Current2: %f, Current3: %f\n", channel2_current1, channel2_current2, channel2_current3);


            /* Send the values via SVPublisher */
            SVPublisher_ASDU_setFLOAT(asdu, amp1, channel1_current1);
            SVPublisher_ASDU_setQuality(asdu, amp1q, q);
            SVPublisher_ASDU_setFLOAT(asdu, amp2, channel1_current2);
            SVPublisher_ASDU_setQuality(asdu, amp2q, q);
            SVPublisher_ASDU_setFLOAT(asdu, amp3, channel1_current3);
            SVPublisher_ASDU_setQuality(asdu, amp3q, q);

            SVPublisher_ASDU_setFLOAT(asdu, amp4, channel2_current1);
            SVPublisher_ASDU_setQuality(asdu, amp4q, q);
            SVPublisher_ASDU_setFLOAT(asdu, amp5, channel2_current2);
            SVPublisher_ASDU_setQuality(asdu, amp5q, q);
            SVPublisher_ASDU_setFLOAT(asdu, amp6, channel2_current3);
            SVPublisher_ASDU_setQuality(asdu, amp6q, q);

            SVPublisher_ASDU_setFLOAT(asdu, vol1, channel1_voltage1);
            SVPublisher_ASDU_setQuality(asdu, vol1q, q);
            SVPublisher_ASDU_setFLOAT(asdu, vol2, channel1_voltage2);
            SVPublisher_ASDU_setQuality(asdu, vol2q, q);
            SVPublisher_ASDU_setFLOAT(asdu, vol3, channel1_voltage3);
            SVPublisher_ASDU_setQuality(asdu, vol3q, q);

            SVPublisher_ASDU_setFLOAT(asdu, vol4, channel2_voltage1);
            SVPublisher_ASDU_setQuality(asdu, vol4q, q);
            SVPublisher_ASDU_setFLOAT(asdu, vol5, channel2_voltage2);
            SVPublisher_ASDU_setQuality(asdu, vol5q, q);
            SVPublisher_ASDU_setFLOAT(asdu, vol6, channel2_voltage3);
            SVPublisher_ASDU_setQuality(asdu, vol6q, q);

            SVPublisher_ASDU_setRefrTm(asdu, Hal_getTimeInMs());
            SVPublisher_ASDU_setSmpCnt(asdu, (uint16_t)sampleCount);

            SVPublisher_publish(svPublisher);

            /* Increment sample count and wrap it every second */
            sampleCount = ((sampleCount + 1) % SAMPLES_PER_SECOND);

            tick_256_us++;  // Increment the tick counter in units of 256us

            if((current_phase < phase_count)&& (end_test == 0))
            {
                // Check if it's time to switch to the next phase
                if ((tick_256_us - phase_start_tick) >= phase_duration_ticks) 
                {
                    printf("[time %u ms] Passing Phase from %d -> %d \n\r",phases[current_phase].duration_ms,current_phase,current_phase+1);
                    current_phase++;
                    phase_duration_ticks = phases[current_phase].duration_ms * 1000 / 256;  // Update duration in ticks
                    phase_start_tick = tick_256_us;
                }
                
                if(current_phase == phase_count)
                {
                    end_test=1;
                    current_phase=phase_count-1;
                }

            }
            /* Increment the loop counter to simulate the passage of time */
            sNbLoop256us++;

            /* Wrap sNbLoop256us after a full signal cycle */
            if (sNbLoop256us >= LOOPS_PER_CYCLE) 
            {
                sNbLoop256us -= LOOPS_PER_CYCLE;
            }
            /* Sleep for approximately 256 microseconds */
            usleep(256);
            //delay_256us();
        }
    } 
    else 
    {
        printf("Cannot start SV publisher!\n");
    }

    SVPublisher_destroy(svPublisher);
    return 0;
}

