#include "goose_receiver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libiec61850/goose_receiver.h"
#include "libiec61850/goose_subscriber.h"
#include "libiec61850/goose_publisher.h"
#include "libiec61850/hal_thread.h"

// Global variables
static GooseReceiver globalGooseReceiver = NULL;
static GoosePublisher globalGoosePublisher = NULL;
static bool isRunning = false;

static void log_message(const char* message) {
    printf("[GOOSE] %s\n", message);
}

static void goose_message_received(GooseSubscriber subscriber, void* parameter) {
    char logMsg[512];
    const char* goID = GooseSubscriber_getGoId(subscriber);
    int stNum = GooseSubscriber_getStNum(subscriber);
    int sqNum = GooseSubscriber_getSqNum(subscriber);

    snprintf(logMsg, sizeof(logMsg), 
             "GOOSE received - ID: %s, ST: %d, SQ: %d",
             goID, stNum, sqNum);
    log_message(logMsg);
}

int goose_receiver_init(const GooseReceiverConfig* config) {
    if (!config || !config->interface || !config->goose_id) {
        log_message("Invalid configuration provided");
        return -1;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), 
             "Initializing on %s for GOOSE ID %s", 
             config->interface, config->goose_id);
    log_message(msg);

    globalGooseReceiver = GooseReceiver_create(config->interface);
    if (!globalGooseReceiver) {
        log_message("Failed to create GooseReceiver");
        return -1;
    }

    GooseSubscriber subscriber = GooseSubscriber_create(config->goose_id, NULL);
    if (!subscriber) {
        log_message("Failed to create GooseSubscriber");
        GooseReceiver_destroy(globalGooseReceiver);
        globalGooseReceiver = NULL;
        return -1;
    }

    GooseReceiver_addSubscriber(globalGooseReceiver, subscriber);
    GooseSubscriber_setListener(subscriber, goose_message_received, globalGooseReceiver);

    return 0;
}

void goose_receiver_start(void) {
    if (!globalGooseReceiver) {
        log_message("Receiver not initialized");
        return;
    }

    if (isRunning) {
        log_message("Receiver already running");
        return;
    }

    log_message("Starting GOOSE receiver thread");
    GooseReceiver_startThreaded(globalGooseReceiver);
    isRunning = true;
}

void goose_receiver_cleanup(void) {
    log_message("Cleaning up");

    if (globalGooseReceiver) {
        if (isRunning) {
            GooseReceiver_stopThreaded(globalGooseReceiver);
            isRunning = false;
        }
        GooseReceiver_destroy(globalGooseReceiver);
        globalGooseReceiver = NULL;
    }
}

bool goose_receiver_is_running(void) {
    return isRunning;
}