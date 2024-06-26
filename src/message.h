#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>
#include <stdio.h>

#define MSG_TYPE_FILE_DATA 1
#define MSG_TYPE_OUTPUT_BINARY 2
#define MSG_TYPE_ACK 3
#define MSG_TYPE_CHECK_READINESS 4
#define MSG_TYPE_SERVER_READY 5
#define MSG_TYPE_SERVER_NOT_READY 6
#define MSG_TYPE_COMPILED_FILE_READY 7

#define MSG_TYPE_FILE_C 99
#define MSG_TYPE_FILE_CPP 323

typedef struct {
    uint16_t type;
    uint32_t length;
} MessageHeader;

void send_message_header(int client_socket, uint32_t data_length, uint16_t FLAG);
MessageHeader receive_message_header(int socket, int *error_flag);
int send_message(int client_socket, const char* data, size_t data_size);

#endif // MESSAGE_H