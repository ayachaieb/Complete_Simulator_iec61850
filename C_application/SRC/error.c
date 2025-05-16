#include "error.h"
#include <stdio.h>
#include <stdlib.h>

static FILE *error_fp = NULL;

void error_init(const char *error_file) {
    error_fp = fopen(error_file, "a");
    if (!error_fp) {
        fprintf(stderr, "Failed to open error file: %s\n", error_file);
        exit(1);
    }
}

void error_report(ErrorCode code, const char *message) {
    if (error_fp) {
        fprintf(error_fp, "[ERROR %d] %s\n", code, message);
        fflush(error_fp);
    }
}

void error_cleanup(void) {
    if (error_fp) {
        fclose(error_fp);
        error_fp = NULL;
    }
}
