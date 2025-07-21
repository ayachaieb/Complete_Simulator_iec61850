/*
 * goose_publisher_example.c
 * Compatible with libiec61850 1.5.1
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>

#include "mms_value.h"
#include "goose_publisher.h"
#include "hal_thread.h"

static volatile sig_atomic_t running = 1;

void sigint_handler(int signum)
{
    running = 0;
}

int main(int argc, char **argv)
{
    const char *interface = NULL;

    if (argc > 1) {
        interface = argv[1];
    }
    else {
        interface = "eth0";
    }

    printf("Using interface %s\n", interface);

    signal(SIGINT, sigint_handler);

    LinkedList dataSetValues = LinkedList_create();

    // Create dataset values
    MmsValue* value1 = MmsValue_newIntegerFromInt32(1234);
    MmsValue* value2 = MmsValue_newBinaryTime(false);
    MmsValue* value3 = MmsValue_newIntegerFromInt32(5678);
    
    LinkedList_add(dataSetValues, value1);
    LinkedList_add(dataSetValues, value2);
    LinkedList_add(dataSetValues, value3);

    CommParameters gooseCommParameters;

    gooseCommParameters.appId = 1000;
    gooseCommParameters.dstAddress[0] = 0x01;
    gooseCommParameters.dstAddress[1] = 0x0c;
    gooseCommParameters.dstAddress[2] = 0xcd;
    gooseCommParameters.dstAddress[3] = 0x01;
    gooseCommParameters.dstAddress[4] = 0x00;
    gooseCommParameters.dstAddress[5] = 0x01;
    gooseCommParameters.vlanId = 0;
    gooseCommParameters.vlanPriority = 4;

    GoosePublisher publisher = GoosePublisher_create(&gooseCommParameters, interface);

    if (publisher) {
        GoosePublisher_setGoCbRef(publisher, "DEP_1Tranche/LLN0$GO$gcbInstProt");
        GoosePublisher_setConfRev(publisher, 1);
        GoosePublisher_setTimeAllowedToLive(publisher, 2000);

        printf("Starting GOOSE publishing. Press Ctrl+C to stop.\n");

        uint32_t stNum = 0;
        
        while (running) {
            stNum++;
            GoosePublisher_setStNum(publisher, stNum);
            
            // Update the timestamp value directly
            MmsValue_setBinaryTime(value2, Hal_getTimeInMs());
            
            if (GoosePublisher_publish(publisher, dataSetValues) == -1) {
                fprintf(stderr, "Error sending GOOSE message!\n");
                break;
            }

            printf("Published GOOSE message with stNum: %u\n", stNum);
            
            Thread_sleep(100);
        }

        printf("\nStopping GOOSE publishing...\n");
        GoosePublisher_destroy(publisher);
    }
    else {
        fprintf(stderr, "Failed to create GOOSE publisher.\n");
        fprintf(stderr, "Please ensure '%s' is a valid interface and run as root.\n", interface);
    }

    LinkedList_destroyDeep(dataSetValues, (LinkedListValueDeleteFunction) MmsValue_delete);

    return 0;
}