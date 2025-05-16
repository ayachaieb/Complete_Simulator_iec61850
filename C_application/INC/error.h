#ifndef ERROR_H
#define ERROR_H

typedef enum {
    ERR_NONE = 0,
    ERR_FILE_OPEN,
    ERR_INVALID_CONFIG,
    ERR_PUBLISH_FAIL,
    ERR_IPC
} ErrorCode;

void error_init(const char *error_file);
void error_report(ErrorCode code, const char *message);
void error_cleanup(void);

#endif
