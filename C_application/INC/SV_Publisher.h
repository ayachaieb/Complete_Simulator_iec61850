#ifndef SV_PUBLISHER_H
#define SV_PUBLISHER_H

#include <stdbool.h> // For bool type
#include "parser.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the SV Publisher module.
 * This function sets up the internal SVPublisher instance and its ASDUs.
 * It must be called before SVPublisher_Module_start().
 *
 * @param interface_name The network interface name (e.g., "eth0") to use for publishing.
 * @return true if initialization is successful, false otherwise.
 */
bool SVPublisher_init(SV_SimulationConfig* instances,int number_publishers);

/**
 * @brief Starts the SV publishing loop in a separate thread.
 * This function should only be called after successful initialization.
 * The publisher will continuously send SV messages until stopped.
 *
 * @return true if the publishing thread is successfully started, false otherwise.
 */
bool SVPublisher_start();

/**
 * @brief Stops the SV publishing loop and cleans up resources.
 * This function can be called to gracefully stop the publisher.
 * It waits for the publishing thread to terminate.
 */
void SVPublisher_stop();

#ifdef __cplusplus
}
#endif

#endif 