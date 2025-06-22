#include "goose_receiver.h"
#include <stdio.h>
#include <unistd.h>

int main() {
    printf("GOOSE Receiver Test Program\n");

    GooseReceiverConfig config = {
        .interface = "eth0",
        .goose_id = "simpleIOGenericGOOSE/LLN0$GO$gcbAnalogValues",
        .enable_retransmission = false,
        .max_retries = 3
    };

    if (goose_receiver_init(&config) != 0) {
        printf("Initialization failed\n");
        return 1;
    }

    goose_receiver_start();
    printf("Receiver started. Waiting for messages...\n");

    // Run for 60 seconds
    for (int i = 0; i < 60; i++) {
        if (!goose_receiver_is_running()) {
            printf("Receiver stopped unexpectedly\n");
            break;
        }
        sleep(1);
    }

    goose_receiver_cleanup();
    printf("Test completed\n");

    return 0;
}