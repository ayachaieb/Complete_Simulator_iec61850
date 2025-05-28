// ipc_module.h
#ifndef IPC_MODULE_H
#define IPC_MODULE_H

#include <sys/socket.h>
#include <sys/un.h>
#include <cjson/cJSON.h>
#include "Ring_Buffer.h" // Assuming Ring_Buffer.h contains EventQueue definition

// Define a callback function type for handling received messages
typedef void (*message_handler_cb)(cJSON *json_message, int client_sock, EventQueue *event_q);

// Initialize the IPC module (create and connect socket)
int ipc_init(const char *socket_path, int *sock);

// Periodically check for and receive messages
void ipc_receive_messages(int sock, message_handler_cb handler_cb, EventQueue *event_q);

// Send a JSON response over the socket
ssize_t ipc_send_json_response(int sock, cJSON *json_response);

// Cleanup IPC resources
void ipc_cleanup(int sock);

#endif // IPC_MODULE_H