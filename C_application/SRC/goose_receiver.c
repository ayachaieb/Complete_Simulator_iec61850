#include "goose_receiver.h"
#include "logging.h"
#include <stdio.h>

int goose_receiver_init(const char *interface, const Config *config) {
    char msg[256];
    snprintf(msg, sizeof(msg), "Initializing GOOSE receiver on %s for %s", interface, config->goose_id);
    log_message(msg);
    /* Initialize libiec61850 GOOSE subscriber */
    return 0;
}

void goose_receiver_start(void) {
    log_message("Starting GOOSE receiver");
    /* Start receiving GOOSE messages */
}

void goose_receiver_cleanup(void) {
    log_message("Cleaning up GOOSE receiver");
}
