#include "logging.h"
#include <stdio.h>
#include <stdlib.h>

static FILE *log_fp = NULL;

void log_init(const char *log_file) {
    log_fp = fopen(log_file, "a");
    if (!log_fp) {
        fprintf(stderr, "Failed to open log file: %s\n", log_file);
        exit(1);
    }
}

void log_message(const char *message) {
    if (log_fp) {
        fprintf(log_fp, "%s\n", message);
        fflush(log_fp);
    }
}

void log_cleanup(void) {
    if (log_fp) {
        fclose(log_fp);
        log_fp = NULL;
    }
}


