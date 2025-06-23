#ifndef GOOSE_PUBLISHER_H
#define GOOSE_PUBLISHER_H
#include "parser.h"

void goose_publisher_send(GOOSE_SimulationConfig* config);
void goose_publisher_cleanup(void);

#endif
