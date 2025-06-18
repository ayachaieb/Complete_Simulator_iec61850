#ifndef GOOSE_PUBLISHER_H
#define GOOSE_PUBLISHER_H

#include <libiec61850/goose_publisher.h>
int goose_publisher_init(const char *interface, const Config *config);
void goose_publisher_send(const char *scenario_file);
void goose_publisher_cleanup(void);

#endif
