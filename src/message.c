#include "message.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

void send_message_header(int client_socket, MessageHeader header) {
    if (send(client_socket, &header, sizeof(MessageHeader), 0) < 0) {
        perror("Sending header failed");
        close(client_socket);
    }
    memset(&header, 0, sizeof(header));
}

