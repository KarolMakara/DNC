//
// Created by kmakara on 04/06/24.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <stdint.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/stat.h>

#include "message.h"
#include "constants.h"
#include "networking.h"


// set socket to non-blocking mode, and set desired timeout
// the purpose of this function is to set timout, because it won't take forever to get socket timout
// when socket is trying to connect not accessible target
int connect_with_timeout(int socket, struct sockaddr_in server_addr, struct timeval timeout) {

    if (socket < 0)
    {
        return -1;
    }

    fd_set Read, Write, Err;

    FD_ZERO(&Read);
    FD_ZERO(&Write);
    FD_ZERO(&Err);
    FD_SET(socket, &Read);
    FD_SET(socket, &Write);
    FD_SET(socket, &Err);


    unsigned long iMode = 1;
    int iResult = ioctl(socket, FIONBIO, &iMode);
    if (iResult != 0)
    {
        return -1;
    }

    int err;
    if (connect(socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
        err = errno;
        if (err != EINPROGRESS ){
            return -1;
        }
    }

    iMode = 0;
    iResult = ioctl(socket, FIONBIO, &iMode);
    if (iResult != 0) {
        return -1;
    }

    if (setsockopt (socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,sizeof(timeout)) < 0) {
        return -1;
    }
    if (setsockopt (socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,sizeof(timeout)) < 0) {
        return -1;
    }

    int n;
    n = select(socket+1,&Read,&Write,NULL,&timeout);
    if ( n <= 0) {
        return -1;
    }
    return 0;
}


int remove_socket_timout(int socket) {
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        return -1;
    }
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        return -1;
    }
    return 0;
}


int receiveACK(int socket) {
    int error_no = 0;
    MessageHeader header = receive_message_header(socket, &error_no);

    if (error_no == -1) {
        close(socket);
        return -1;
    }

    if (header.type != MSG_TYPE_ACK) {
        fprintf(stderr, "Expected ACK, received message type: %u\n", header.type);
        return -1;
    }

    if (header.length != 0) {
        fprintf(stderr, "Invalid ACK length: %u\n", header.length);
        return -1;
    }

    printf("Received ACK from server\n");
    return 0;
}


int isServerReady(int client_socket) {

    send_message_header(client_socket, 0, MSG_TYPE_CHECK_READINESS);

    int error_no = 0;
    MessageHeader header = receive_message_header(client_socket, &error_no);

    if (error_no == -1) {
        close(client_socket);
        return -1;
    }

    if (header.type == MSG_TYPE_SERVER_READY) {
        return 0;
    } else if (header.type == MSG_TYPE_SERVER_NOT_READY) {
        printf("Server is not ready for compilation.\n");
        return -1;
    } else {
        fprintf(stderr, "Unexpected message type: %u\n", header.type);
        return -1;
    }
}


int sendFileToServer(int client_socket, const char *data, size_t data_length, const char *output_binary) {

    send_message_header(client_socket, (uint32_t)data_length, MSG_TYPE_FILE_DATA);

    if (send_message(client_socket, data, data_length) == -1) {
        fprintf(stderr, "Failed to send file data.\n");
        return -1;
    }


    if (receiveACK(client_socket) == -1) {
        printf("Client did not received ACK message.\n");
        return -1;
    }

    size_t output_binary_length = strlen(output_binary);

    send_message_header(client_socket, (uint32_t)output_binary_length, MSG_TYPE_OUTPUT_BINARY);

    if (send_message(client_socket, output_binary, output_binary_length) == -1) {
        fprintf(stderr, "Failed to send output binary name.\n");
        return -1;
    }

    return 0;
}


void receiveCompiledFile(int client_socket, const char *output_file_path) {
    char *buffer = malloc(INITIAL_BUFFER_SIZE);
    if (buffer == NULL) {
        perror("Error allocating memory");
        return;
    }

    size_t buffer_size = INITIAL_BUFFER_SIZE;
    size_t bytes_received;

    printf("DEBUG output_file_path %s\n", output_file_path);

    char *last_slash = strrchr(output_file_path, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';
        if (mkdir(output_file_path, 0777) == -1 && errno != EEXIST) {
            perror("Error creating directory");
            free(buffer);
            return;
        }
        *last_slash = '/';
    }

    FILE *output_file = fopen(output_file_path, "wb");
    if (output_file == NULL) {
        perror("Error opening file for writing");
        free(buffer);
        return;
    }

    int flags = fcntl(client_socket, F_GETFL, 0);
    fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);

    fd_set read_fds;

    FD_ZERO(&read_fds);
    FD_SET(client_socket, &read_fds);


    int select_result = select(client_socket + 1, &read_fds, NULL, NULL, NULL);
    if (select_result == -1) {
        perror("Error in select");
    } else if (select_result == 0) {
        printf("Timeout occurred while waiting for data.\n");
    }

    size_t total_received = 0;

    while (1) {


        bytes_received = recv(client_socket, buffer, buffer_size, 0);
        if (bytes_received == 0) {
            break;
        }

        fwrite(buffer, 1, bytes_received, output_file);
        total_received += bytes_received;

        if (bytes_received == buffer_size) {
            buffer_size *= 2;
            char *new_buffer = realloc(buffer, buffer_size);
            if (new_buffer == NULL) {
                perror("Error reallocating memory");
                break;
            }
            buffer = new_buffer;
        }
    }

    printf("Saved file %s\n", output_file_path);

    fclose(output_file);
    free(buffer);

// Set the socket back to blocking mode
    fcntl(client_socket, F_SETFL, flags);
}