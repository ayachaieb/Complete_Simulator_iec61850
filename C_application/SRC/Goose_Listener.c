#include "util.h"
#include "hal_thread.h"
#include "linked_list.h"
#include "State_Machine.h"
#include "Goose_Listener.h"
#include "goose_receiver.h"
#include "goose_subscriber.h"
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include "parser.h"
volatile sig_atomic_t running_Goose = 1;
extern volatile bool internal_shutdown_flag;
GooseReceiver receiver = NULL;
bool goose_receiver_cleanup(void);
bool goose_receiver_is_running(void);

GooseReceiverConfig receiverConfig = {0};
 GooseSubscriber subscriber = NULL;
static void
sigint_handler(int signalId)
{
    running_Goose = 0;
    StateMachine_push_event(STATE_EVENT_shutdown, NULL, NULL);
    internal_shutdown_flag = true; // Signal ipc_run_loop to exit
    printf("Received SIGINT, shutting down...\n");
    goose_receiver_cleanup(); // Clean up GOOSE receiver resources
    printf("GOOSE receiver  cleaned up.\n");
    exit(0); // Exit the program
}

static void
gooseListener(GooseSubscriber subscriber, void* parameter)
{
    printf("GOOSE event:\n");
    printf("  stNum: %u sqNum: %u\n", GooseSubscriber_getStNum(subscriber),
            GooseSubscriber_getSqNum(subscriber));
    printf("  timeToLive: %u\n", GooseSubscriber_getTimeAllowedToLive(subscriber));

    uint64_t timestamp = GooseSubscriber_getTimestamp(subscriber);

    printf("  timestamp: %u.%u\n", (uint32_t) (timestamp / 1000), (uint32_t) (timestamp % 1000));
    printf("  message is %s\n", GooseSubscriber_isValid(subscriber) ? "valid" : "INVALID");

    MmsValue* values = GooseSubscriber_getDataSetValues(subscriber);

    char buffer[1024];

    MmsValue_printToBuffer(values, buffer, 1024);

    printf("  allData: %s\n", buffer);
}


bool goose_receiver_cleanup(void)
{
    // Clean up GOOSE receiver resources
    if (receiver != NULL) {
        GooseReceiver_stop(receiver);
        GooseReceiver_destroy(receiver);
        receiver = NULL; // Set to NULL after destruction
    }
    printf("GOOSE receiver and subscriber cleaned up.\n");
    return SUCCESS;
}

bool goose_receiver_is_running_Goose(void) {
  
    return receiver != NULL;
}


int Goose_receiver_init(SV_SimulationConfig config)
{
    int retval = SUCCESS;
   if (receiver == NULL) 
   {
        receiver = GooseReceiver_create();
    }


   
    receiverConfig.interface = config.Interface;
    receiverConfig.goose_id = config.GoID;
    receiverConfig.GoCBRef = config.GoCBRef;
    receiverConfig.DatSet = config.DatSet;
    receiverConfig.MACAddress = config.MACAddress;
    receiverConfig.AppID = config.AppID;
    receiverConfig.enable_retransmission = true;
    receiverConfig.max_retries = 3;
    printf("DEBUG: receiverConfig.interface: %s\n", receiverConfig.interface);
  
    GooseReceiver_setInterfaceId(receiver, receiverConfig.interface);
    // GooseReceiver_setGooseId(receiver, receiverConfig.goose_id);
    // GooseReceiver_setGoCBRef(receiver, receiverConfig.GoCBRef);
    // GooseReceiver_setDatSet(receiver, receiverConfig.DatSet);
    // GooseReceiver_setDstMac(receiver, receiverConfig.MACAddress);
    // GooseReceiver_setAppId(receiver, receiverConfig.AppID);

    subscriber = GooseSubscriber_create("simpleIOGenericIO/LLN0$GO$gcbAnalogValues", NULL);

    uint16_t appid = (uint32_t)strtoul(receiverConfig.AppID, NULL, 10);
    uint8_t dstMac[6] ;
    parse_mac_address(config.MACAddress, dstMac);
    GooseSubscriber_setDstMac(subscriber,dstMac);

    GooseSubscriber_setAppId(subscriber,appid );

    GooseSubscriber_setListener(subscriber, gooseListener, NULL);
    GooseReceiver_addSubscriber(receiver, subscriber);

    return (retval);
}

int Goose_receiver_start()
{
    GooseReceiver_start(receiver);

    signal(SIGINT, sigint_handler);

   if (GooseReceiver_isRunning(receiver)) {
        signal(SIGINT, sigint_handler);

        while (running_Goose) {
            Thread_sleep(100);
        }
    }
    else {
        printf("Failed to start GOOSE subscriber. Reason can be that the Ethernet interface doesn't exist or root permission are required.\n");
    }

    goose_receiver_cleanup();
    return 0;
}
