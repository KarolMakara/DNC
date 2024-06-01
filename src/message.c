#include "message.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

void send_message_header(int client_socket, uint32_t data_length, uint8_t FLAG) {

    MessageHeader header = {};
    memset(&header, 0, sizeof(header));

    header.type = FLAG;
    header.length = data_length;

    if (send(client_socket, &header, sizeof(MessageHeader), 0) < 0) {
        perror("Sending header failed");
        close(client_socket);
    }
    printf("Sent header: %hhu, %hhu\n", header.type, header.length);
}

MessageHeader receive_message_header(int socket, int *error_flag) {
    MessageHeader header;
    ssize_t bytes_received = recv(socket, &header, sizeof(MessageHeader), 0);
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            fprintf(stderr, "Client disconnected\n");
        } else {
            perror("Error reading message header");
        }
        *error_flag = -1;
    } else {
        *error_flag = 0;
    }

    printf("Received header: %hhu, %hhu\n", header.type, header.length);

    return header;
}

int send_message(int client_socket, const char* data, size_t data_size) {
    if (send(client_socket, data, data_size, 0) < 0) {
        perror("Sending message failed");
        close(client_socket);
        return -1;
    }
    return 0;
}
