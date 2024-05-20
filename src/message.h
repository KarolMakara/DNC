#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>

#define MSG_TYPE_FILE_DATA 1
#define MSG_TYPE_OUTPUT_BINARY 2
#define MSG_TYPE_ACK 3

typedef struct {
    uint8_t type;
    uint32_t length;
} MessageHeader;

#endif // MESSAGE_H