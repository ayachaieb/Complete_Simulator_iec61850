#ifndef SV_PUBLISHER_H
#define SV_PUBLISHER_H

#include <libiec61850/sv_publisher.h>
int sv_publisher_init(const char *interface, const Config *config);
void sv_publisher_send(const char *scenario_file);
void sv_publisher_cleanup(void);

#endif
