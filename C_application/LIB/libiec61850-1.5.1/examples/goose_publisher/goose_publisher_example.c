/*
 * goose_publisher_example.c
 *
 * Modified to send GOOSE messages continuously.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h> // Required for signal handling

#include "mms_value.h"
#include "goose_publisher.h"
#include "hal_thread.h" // For Thread_sleep

/* Global flag to control the main loop */
static volatile sig_atomic_t running = 1;

/* Signal handler for Ctrl+C */
void
sigint_handler(int running) {
    running = 0;
}

/* has to be executed as root! */
int
main(int argc, char **argv)
{
    const char *interface = NULL;


    if (argc > 1)
        interface = argv[1];
    else
        interface = "lo"; // Default interface if not provided as argument

    printf("Using interface %s\n", interface);

    // Register the signal handler for SIGINT (Ctrl+C)
    signal(SIGINT, sigint_handler);

    LinkedList dataSetValues = LinkedList_create();

    // Initial dataset values
    LinkedList_add(dataSetValues, MmsValue_newIntegerFromInt32(1234));
    LinkedList_add(dataSetValues, MmsValue_newBinaryTime(false)); // Represents a timestamp, usually current time
    LinkedList_add(dataSetValues, MmsValue_newIntegerFromInt32(5678));

    CommParameters gooseCommParameters;

    gooseCommParameters.appId = 1000;
    gooseCommParameters.dstAddress[0] = 0x01;
    gooseCommParameters.dstAddress[1] = 0x0c;
    gooseCommParameters.dstAddress[2] = 0xcd;
    gooseCommParameters.dstAddress[3] = 0x01;
    gooseCommParameters.dstAddress[4] = 0x00;
    gooseCommParameters.dstAddress[5] = 0x01;
    gooseCommParameters.vlanId = 0;        // Set to 0 if no VLAN is used
    gooseCommParameters.vlanPriority = 4; // Default priority

    /*
     * Create a new GOOSE publisher instance.
     */
    GoosePublisher publisher = GoosePublisher_create(&gooseCommParameters, interface);

    if (publisher) {
        GoosePublisher_setGoCbRef(publisher, "simpleIOGenericIO/LLN0$GO$gcbAnalogValues");
        GoosePublisher_setConfRev(publisher, 1);
        GoosePublisher_setDataSetRef(publisher, "simpleIOGenericIO/LLN0$AnalogValues");
        GoosePublisher_setTimeAllowedToLive(publisher, 500); // Time in ms after which a message is considered stale

        printf("Starting GOOSE publishing. Press Ctrl+C to stop.\n");

        while (running) { // Loop indefinitely until 'running' flag is cleared by signal handler
            // You can update dataSetValues here if you want to send changing data
            // For example, to simulate changing analog values:
            // MmsValue_setIntegerFromInt32(LinkedList_get(dataSetValues, 0), (int32_t) (rand() % 10000));
            // MmsValue_setBinaryTime(LinkedList_get(dataSetValues, 1), false); // Update timestamp if needed

            if (GoosePublisher_publish(publisher, dataSetValues) == -1) {
                printf("Error sending GOOSE message!\n");
            } else {
                // printf("GOOSE message sent.\n"); // Uncomment for verbose output
            }

            Thread_sleep(1); // Publish every 100 milliseconds (adjust as needed)
                               // GOOSE messages often have a specific retransmission rate
        }

        printf("Stopping GOOSE publishing...\n");
        GoosePublisher_destroy(publisher);
    }
    else {
        printf("Failed to create GOOSE publisher. Reason: Ethernet interface doesn't exist or root permissions are required.\n");
        printf("Please ensure '%s' is a valid and active network interface, and run as root (sudo).\n", interface);
    }

    LinkedList_destroyDeep(dataSetValues, (LinkedListValueDeleteFunction) MmsValue_delete);

    return 0;
}