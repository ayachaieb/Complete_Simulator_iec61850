#ifndef IPC_H
#define IPC_H

#include <stddef.h> // For size_t
#include "State_Machine.h" // Assuming this defines state_event_e

// Callback function type for delivering events from the IPC module to the main application logic.
// The `event` parameter indicates the mapped state machine event.
// The `requestId` parameter (can be NULL) carries the original requestId from the incoming JSON.
typedef void (*ipc_event_callback_t)(state_event_e event, const char *requestId);

/**
 * @brief Initializes the IPC socket module.
 *
 * This function creates a Unix domain socket, sets it to non-blocking mode,
 * and attempts to connect to the specified SOCKET_PATH.
 *
 * @param event_cb A callback function to be invoked when an incoming IPC message
 * is successfully parsed and mapped to a state machine event.
 * This callback bridges the IPC module to your application's
 * event processing (e.g., pushing to a state machine queue).
 * @return 0 on successful initialization and connection (or connection in progress),
 * -1 on failure (e.g., socket creation failed, failed to set non-blocking,
 * or connection failed for reasons other than EINPROGRESS).
 */
int ipc_init(ipc_event_callback_t event_cb);

/**
 * @brief Runs the main communication loop for the IPC socket.
 *
 * This function continuously attempts to receive data from the connected Unix
 * domain socket. It operates in a non-blocking manner.
 *
 * Incoming JSON messages are parsed. Based on the "type" field, a corresponding
 * `state_event_e` is determined. The `requestId` from the "data" field (if present)
 * is also extracted. These are then passed to the `ipc_event_callback_t`
 * provided during initialization.
 *
 * The loop continues until `ipc_shutdown()` is called or a fatal
 * error occurs during reception.
 *
 * @param shutdown_check_func A function pointer to a function that returns 1 if
 * the main application loop should shut down, 0 otherwise. This allows the IPC
 * module to respect the global shutdown state.
 * @return 0 on graceful shutdown (e.g., `ipc_shutdown` called or
 * remote disconnected), -1 on a critical error during reception.
 */
int ipc_run_loop(int (*shutdown_check_func)(void));

/**
 * @brief Signals the IPC socket module to shut down its operations.
 *
 * This function sets an internal flag to terminate the `ipc_run_loop()`
 * and attempts to close the socket file descriptor.
 */
void ipc_shutdown(void);

/**
 * @brief Sends a JSON response string back to the connected client.
 *
 * This function is typically called by the application's event handler (the
 * `ipc_event_callback_t`) after processing an incoming request, to send
 * acknowledgement or results back to the client. It includes a simulated delay.
 *
 * @param response_json A null-terminated C string containing the JSON response.
 * @return 0 on success, -1 on failure (e.g., socket not initialized, send error).
 */
int ipc_send_response(const char *response_json);

#endif // IPC_SOCKET_MODULE_H