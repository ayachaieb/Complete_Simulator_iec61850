#ifndef GOOSE_LISTENER_H
#define GOOSE_LISTENER_H

#include <stdbool.h>
#include <stdint.h>
#include "parser.h"

// Configuration structure for GOOSE receiver
typedef struct {
    const char* interface;       // Network interface (e.g., "eth0")
    const char* goose_id;        // GOOSE control block ID to listen for
    const char* GoCBRef; // Reference to the GOOSE Control Block
    const char* DatSet;  // Data Set reference
    const char* MACAddress; // MAC address for GOOSE communication
    const char* AppID;  // Application ID for GOOSE
    bool enable_retransmission;  // Whether to enable message retransmission
    int max_retries;             // Maximum retransmission attempts
} GooseReceiverConfig;

// Function prototypes
//int goose_receiver_init(const GooseReceiverConfig* config);
int Goose_receiver_init(SV_SimulationConfig config);
void goose_receiver_start();
bool goose_receiver_cleanup(void);
bool goose_receiver_is_running(void);

#endif // GOOSE_LISTENER_H