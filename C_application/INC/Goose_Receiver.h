#ifndef GOOSE_RECEIVER_H
#define GOOSE_RECEIVER_H

#include <libiec61850/goose_subscriber.h>
int goose_receiver_init(const char *interface, const Config *config);
void goose_receiver_start(void);
void goose_receiver_cleanup(void);

#endif
