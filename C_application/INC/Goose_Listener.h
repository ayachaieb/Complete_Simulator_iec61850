#ifndef GOOSE_LISTENER_H
#define GOOSE_LISTENER_H

#include <stdbool.h>
#include <stdint.h>
#include "parser.h"
#include "Goose_Listener.h"
#include "goose_receiver.h"
#include "goose_subscriber.h"
// Configuration structure for GOOSE receiver
typedef struct {
    const char* interface;       // Network interface (e.g., "eth0")*
    uint32_t goose_id;        // GOOSE appid
    const char* GoCBRef; // Reference to the GOOSE Control Block*
    const char* DatSet;  // Data Set reference
    const uint8_t* MACAddress; // MAC address for GOOSE communication
    uint32_t AppID;  // Application ID 
    GooseReceiver receiver ;
    GooseSubscriber subscriber ; // GOOSE subscriber instance
    bool enable_retransmission;  // Whether to enable message retransmission
    int max_retries;             // Maximum retransmission attempts
} ThreadData;

// Function prototypes
//int goose_receiver_init(const GooseReceiverConfig* config);
int Goose_receiver_init(SV_SimulationConfig* config,int number_of_subscribers);
void goose_receiver_start();
int goose_receiver_cleanup(void);
bool goose_receiver_is_running(void);

#endif // GOOSE_LISTENER_H