/*
 * goose_publisher_example.c
 *
 * Modified to send GOOSE messages continuously.
 * Rectified to handle signals correctly and fix sleep duration.
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

/* Global flag to control the main loop.
 * 'volatile' ensures the compiler doesn't optimize away reads of this variable.
 * 'sig_atomic_t' ensures that access is atomic with respect to signals.
 */
static volatile sig_atomic_t running = 1;

/*
 * Signal handler for SIGINT (Ctrl+C).
 * The function signature for a signal handler is void handler(int signum).
 */
void
sigint_handler(int signum)
{
    /* Set the global 'running' flag to 0 to terminate the main loop. */
    running = 0;
}

/* has to be executed as root! */
int
main(int argc, char **argv)
{
    const char *interface = NULL;

    if (argc > 1) {
        interface = argv[1];
    }
    else {
        interface = "lo"; // Default interface if not provided as an argument
    }

    printf("Using interface %s\n", interface);

    // Register the signal handler for SIGINT (Ctrl+C)
    signal(SIGINT, sigint_handler);

    LinkedList dataSetValues = LinkedList_create();

    // Initial dataset values
    LinkedList_add(dataSetValues, MmsValue_newIntegerFromInt32(1234));
    LinkedList_add(dataSetValues, MmsValue_newBinaryTime(false));
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
        GoosePublisher_setGoCbRef(publisher, "DEP_1Tranche/LLN0$GO$gcbInstProt");
       // GoosePublisher_setDataSetRef(publisher, "simpleIOGenericIO/LLN0$DS$dsAnalogValues");
        GoosePublisher_setConfRev(publisher, 1);

        printf("Starting GOOSE publishing. Press Ctrl+C to stop.\n");

        
        while (running) {
            if (GoosePublisher_publish(publisher, dataSetValues) == -1) {
                fprintf(stderr, "Error sending GOOSE message!\n");
                break; // Exit loop on publish error
            }

            /*
             * The Thread_sleep() function takes milliseconds as an argument.
             * Sleeping for 100ms is a more reasonable interval than 1ms.
             */
            Thread_sleep(100);
        }

        printf("\nStopping GOOSE publishing...\n");
        GoosePublisher_destroy(publisher);
    }
    else {
        fprintf(stderr, "Failed to create GOOSE publisher. Reason: Ethernet interface doesn't exist or root permissions are required.\n");
        fprintf(stderr, "Please ensure '%s' is a valid and active network interface, and run as root (sudo).\n", interface);
    }

    LinkedList_destroyDeep(dataSetValues, (LinkedListValueDeleteFunction) MmsValue_delete);

    return 0;
}