/*
 * goose_observer.c
 *
 * This is an example for a GOOSE observer that tries to precisely
 * match the publisher's configuration, assuming a libiec61850
 * version without GooseFilterConfig.
 *
 * Has to be started as root in Linux.
 */

#include "goose_receiver.h"
#include "goose_subscriber.h"
#include "hal_thread.h" // For Thread_sleep

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For memcpy (though not strictly needed here anymore, good practice)

static int running = 1;

void sigint_handler(int signalId)
{
    running = 0;
}


void
gooseListener(GooseSubscriber subscriber, void* parameter)
{
    printf("\n--- GOOSE Event Received ---\n");
    fflush(stdout);
     printf(" Message validity: %s\n", GooseSubscriber_isValid(subscriber) ? "valid" : "INVALID");
    
     // Rest of the existing code
    printf("  vlanTag: %s\n", GooseSubscriber_isVlanSet(subscriber) ? "found" : "NOT found");
    if (GooseSubscriber_isVlanSet(subscriber))
    {
        printf("    vlanId: %u\n", GooseSubscriber_getVlanId(subscriber));
        printf("    vlanPrio: %u\n", GooseSubscriber_getVlanPrio(subscriber));
    }
    printf("  appId: %d\n", GooseSubscriber_getAppId(subscriber));
    uint8_t macBuf[6];
    GooseSubscriber_getSrcMac(subscriber,macBuf);
    printf("  srcMac: %02X:%02X:%02X:%02X:%02X:%02X\n", macBuf[0],macBuf[1],macBuf[2],macBuf[3],macBuf[4],macBuf[5]);
    GooseSubscriber_getDstMac(subscriber,macBuf);
    printf("  dstMac: %02X:%02X:%02X:%02X:%02X:%02X\n", macBuf[0],macBuf[1],macBuf[2],macBuf[3],macBuf[4],macBuf[5]);
    printf("  goId: %s\n", GooseSubscriber_getGoId(subscriber));
    printf("  goCbRef: %s\n", GooseSubscriber_getGoCbRef(subscriber));
    printf("  dataSet: %s\n", GooseSubscriber_getDataSet(subscriber));
    printf("  confRev: %u\n", GooseSubscriber_getConfRev(subscriber));
    printf("  ndsCom: %s\n", GooseSubscriber_needsCommission(subscriber) ? "true" : "false");
    printf("  simul: %s\n", GooseSubscriber_isTest(subscriber) ? "true" : "false");
    printf("  stNum: %u sqNum: %u\n", GooseSubscriber_getStNum(subscriber),
             GooseSubscriber_getSqNum(subscriber));
    printf("  timeToLive: %u\n", GooseSubscriber_getTimeAllowedToLive(subscriber));

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
}



int main(int argc, char** argv)
{
 GooseReceiver receiver = GooseReceiver_create();

 const char *interfaceId = NULL;
 if (argc > 1) {
 interfaceId = argv[1];
 } else {
 interfaceId = "lo";
 }
 printf("Using interface %s\n", interfaceId);
 GooseReceiver_setInterfaceId(receiver, interfaceId);

 const char* PUBLISHER_GO_ID = "simpleIOGenericIO/LLN0$GO$gcbAnalogValues";
 GooseSubscriber subscriber = GooseSubscriber_create(PUBLISHER_GO_ID, NULL);

 if (subscriber == NULL) {
 printf("Failed to create GOOSE subscriber.\n");
 GooseReceiver_destroy(receiver);
 return -1;
 }

 // Explicitly set parameters to match publisher
 uint8_t dstMac[6] = {0x01, 0x0C, 0xCD, 0x01, 0x00, 0x01};
 GooseSubscriber_setDstMac(subscriber, dstMac);
 GooseSubscriber_setAppId(subscriber, 1000);


 GooseSubscriber_setListener(subscriber, gooseListener, NULL);
 GooseReceiver_addSubscriber(receiver, subscriber);

 GooseReceiver_start(receiver);

 if (GooseReceiver_isRunning(receiver)) {
 signal(SIGINT, sigint_handler);
 printf("GOOSE observer started. Listening for messages with goID '%s'...\n", PUBLISHER_GO_ID);
 while (running) {
 Thread_sleep(100);
 }
 } else {
 printf("Failed to start GOOSE receiver. Check interface '%s' and root permissions.\n", interfaceId);
 }

 printf("Stopping GOOSE observer...\n");
 GooseReceiver_stop(receiver);
 GooseReceiver_destroy(receiver);
 return 0;
}
