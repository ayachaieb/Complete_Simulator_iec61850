#ifndef GOOSE_RECEIVER_H
#define GOOSE_RECEIVER_H

#include <stdbool.h>
#include <stdint.h>

// Configuration structure for GOOSE receiver
typedef struct {
    const char* interface;       // Network interface (e.g., "eth0")
    const char* goose_id;        // GOOSE control block ID to listen for
    bool enable_retransmission;  // Whether to enable message retransmission
    int max_retries;             // Maximum retransmission attempts
} GooseReceiverConfig;

// Function prototypes
int goose_receiver_init(const GooseReceiverConfig* config);
void goose_receiver_start(void);
void goose_receiver_cleanup(void);
bool goose_receiver_is_running(void);

#endif // GOOSE_RECEIVER_H