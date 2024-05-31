#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>

#define MSG_TYPE_FILE_DATA 1
#define MSG_TYPE_OUTPUT_BINARY 2
#define MSG_TYPE_ACK 3
#define MSG_TYPE_CHECK_READINESS 4
#define MSG_TYPE_SERVER_READY 5
#define MSG_TYPE_SERVER_NOT_READY 6
#define MSG_TYPE_COMPILED_FILE_READY 7

typedef struct {
    uint8_t type;
    uint32_t length;
} MessageHeader;

void send_message_header(int client_socket, MessageHeader header);

#endif // MESSAGE_H