#ifndef LOGGING_H
#define LOGGING_H

void log_init(const char *log_file);
void log_message(const char *message);
void log_cleanup(void);

#endif
