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


#include "message.h"
#include "constants.h"
#include "networking.h"
#include "helpers.h"


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


int receive_ACK(int socket) {
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


int is_server_ready(int client_socket) {

    send_message_header(client_socket, 0, MSG_TYPE_CHECK_READINESS);

    int error_no = 0;
    MessageHeader header = receive_message_header(client_socket, &error_no);

    if (error_no == -1) {
        close(client_socket);
        return -1;
    }

    if (header.type == MSG_TYPE_SERVER_READY) {
        printf("Server is ready for compilation.\n");
        return 0;
    } else if (header.type == MSG_TYPE_SERVER_NOT_READY) {
        printf("Server is not ready for compilation.\n");
        return -1;
    } else {
        fprintf(stderr, "Unexpected message type: %u\n", header.type);
        return -1;
    }
}


int send_file_to_server(int client_socket, const char *data, size_t data_length, const char *output_binary, uint16_t file_type) {

    send_message_header(client_socket, (uint32_t)data_length, MSG_TYPE_FILE_DATA);

    if (send_message(client_socket, data, data_length) == -1) {
        fprintf(stderr, "Failed to send file data.\n");
        return -1;
    }


    if (receive_ACK(client_socket) == -1) {
        printf("Client did not received ACK message.\n");
        return -1;
    }

    send_message_header(client_socket, 0, file_type);

    size_t output_binary_length = strlen(output_binary);
    send_message_header(client_socket, (uint32_t)output_binary_length, MSG_TYPE_OUTPUT_BINARY);

    if (send_message(client_socket, output_binary, output_binary_length) == -1) {
        fprintf(stderr, "Failed to send output binary name.\n");
        return -1;
    }

    return 0;
}


void receive_compiled_file(int client_socket, const char *output_file_path) {
    char *buffer = malloc(INITIAL_BUFFER_SIZE);
    if (buffer == NULL) {
        perror("Error allocating memory");
        return;
    }

    size_t buffer_size = INITIAL_BUFFER_SIZE;
    size_t bytes_received;

    create_directory(output_file_path, -1);

    FILE *output_file = fopen(output_file_path, "wb");
    if (output_file == NULL) {
        perror("Error opening file for writing");
        free(buffer);
        return;
    }

//    int flags = fcntl(client_socket, F_GETFL, 0);
//    fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);
//
//    fd_set read_fds;
//
//    FD_ZERO(&read_fds);
//    FD_SET(client_socket, &read_fds);


//    int select_result = select(client_socket + 1, &read_fds, NULL, NULL, NULL);
//    if (select_result == -1) {
//        perror("Error in select");
//    } else if (select_result == 0) {
//        printf("Timeout occurred while waiting for data.\n");
//    }

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

    fclose(output_file);
    free(buffer);

// Set the socket back to blocking mode
//    fcntl(client_socket, F_SETFL, flags);
}


char* receive_file(int client_socket, size_t data_length) {
    size_t buffer_size = 1024;
    char *buffer = (char *)malloc(buffer_size);
    if (buffer == NULL) {
        perror("Error allocating memory for buffer");
        return NULL;
    }


    char *file_data = (char *)malloc(data_length + 1);
    if (file_data == NULL) {
        perror("Error allocating memory for file data");
        free(buffer);
        return NULL;
    }

    size_t total_received = 0;
    while (total_received < data_length) {
        size_t bytes_read = read(client_socket, buffer, buffer_size);
        if (bytes_read <= 0) {
            perror("Error reading file data");
            free(file_data);
            free(buffer);
            return NULL;
        }

        size_t remaining_bytes = data_length - total_received;
        size_t bytes_to_copy = bytes_read < remaining_bytes ? bytes_read : remaining_bytes;
        memcpy(file_data + total_received, buffer, bytes_to_copy);
        total_received += bytes_to_copy;

        if (bytes_read == buffer_size) {
            buffer_size *= 2;
            char *new_buffer = (char *)realloc(buffer, buffer_size);
            if (new_buffer == NULL) {
                perror("Error reallocating memory for buffer");
                free(file_data);
                free(buffer);
                close(client_socket);
                return NULL;
            }
            buffer = new_buffer;
        }
    }
    file_data[data_length] = '\0';

    char file_hash[65];
    sha256(file_data, file_hash);

    char *worker_dir = (char *)malloc(strlen(SERVER_DIRECTORY) + 2 + 1);
    if (worker_dir == NULL) {
        perror("Error allocating memory for file path");
        free(file_data);
        free(buffer);
        return NULL;
    }

    snprintf(worker_dir, strlen(SERVER_DIRECTORY) + 2 + 1, "%s/%zu", SERVER_DIRECTORY, client_count);

    create_directory(worker_dir, 0);


    char *file_path = (char *)malloc(strlen(worker_dir) + strlen(file_hash) + 4 + 1);
    if (file_path == NULL) {
        perror("Error allocating memory for file path");
        free(file_data);
        free(buffer);
        free(worker_dir);
        return NULL;
    }

    snprintf(file_path, strlen(worker_dir) +
                        strlen(file_hash) + 4 + 1, "%s/%s.c", worker_dir, file_hash);


    FILE *file = fopen(file_path, "w");
    if (file == NULL) {
        perror("Error opening file for writing");
        free(file_data);
        free(file_path);
        free(buffer);
        free(worker_dir);
        return NULL;
    }
    fputs(file_data, file);
    fclose(file);

    free(file_data);
    free(buffer);
    free(worker_dir);
    return file_path;
}

void handle_client(int client_socket) {

    char* file_path = NULL;


    int error_no = 0;
    MessageHeader header = receive_message_header(client_socket, &error_no);

    if (error_no == -1) {
        return;
    }



    if (header.type == MSG_TYPE_FILE_DATA) {
        file_path = receive_file(client_socket, header.length);
    } else {
        perror("Unexpected message type1");
        return;
    }


    if (file_path == NULL) {
        close(client_socket);
        free(file_path);
        return;
    } else {
    }

    send_message_header(client_socket, 0, MSG_TYPE_ACK);

    uint16_t file_type;
    error_no = 0;
    header = receive_message_header(client_socket, &error_no);
    file_type = header.type;

    size_t bytes_received = 0;

    while (bytes_received < sizeof(MessageHeader)) {
        ssize_t valread = read(client_socket, ((char *)&header) + bytes_received, sizeof(MessageHeader) - bytes_received);
        if (valread <= 0) {
            perror("Error reading message header");
            close(client_socket);
            return;
        }
        bytes_received += valread;
    }

    if (header.type != MSG_TYPE_OUTPUT_BINARY) {
        fprintf(stderr, "Invalid message type: %d\n", header.type);
        close(client_socket);
        return;
    }

    size_t output_binary_length = header.length;
    char *output_binary = (char *)malloc(output_binary_length + 1);


    size_t buffer_size = INITIAL_BUFFER_SIZE;
    char *buffer = (char *)malloc(buffer_size);
    if (buffer == NULL) {
        perror("Error allocating memory for buffer");
        free(output_binary);
        close(client_socket);
        return;
    }

    if (output_binary == NULL) {
        perror("Error allocating memory for output binary name");
        free(buffer);  // Add this line
        close(client_socket);
        return;
    }

    bytes_received = 0;
    while (bytes_received < output_binary_length) {
        ssize_t valread = read(client_socket, buffer, buffer_size);
        if (valread <= 0) {
            if (valread == 0) {
                fprintf(stderr, "Client closed the connection unexpectedly.\n");
            } else {
                perror("Error reading output binary name");
            }
            free(output_binary);
            free(buffer);
            close(client_socket);
            return;
        }
        size_t remaining_bytes = output_binary_length - bytes_received;
        size_t bytes_to_copy = (size_t)valread < remaining_bytes ? (size_t)valread : remaining_bytes;
        memcpy(output_binary + bytes_received, buffer, bytes_to_copy);
        bytes_received += bytes_to_copy;

        if (bytes_received == buffer_size) {
            buffer_size *= 2;
            char *new_buffer = (char *)realloc(buffer, buffer_size);
            if (new_buffer == NULL) {
                perror("Error reallocating memory for buffer");
                free(output_binary);
                free(buffer);
                close(client_socket);
                return;
            }
            buffer = new_buffer;
        }
    }
    output_binary[output_binary_length] = '\0';


    char path_to_output_binary[MAX_BINARY_NAME_LENGTH];
    snprintf(path_to_output_binary, MAX_BINARY_NAME_LENGTH, "%s/%zu/%s", SERVER_DIRECTORY, client_count, output_binary);


    char command[MAX_COMMAND_LENGTH];

    char command_holder[16];


    if (file_type == MSG_TYPE_FILE_C) {
        strcpy(command_holder, "gcc -c %s %s %s");
    } else if (file_type == MSG_TYPE_FILE_CPP) {
        strcpy(command_holder, "g++ -c %s %s %s");
    } else {
        perror("No compiler specified!");
        close(client_socket);
        free(buffer);
        free(output_binary);
        return;
    }

    snprintf(command, MAX_COMMAND_LENGTH, command_holder, file_path, "-o", path_to_output_binary);

    int result = system(command);
    printf("%s\n", command);
    if (result != 0) {
        perror("Compilation failed");
        close(client_socket);
        free(buffer);
        free(output_binary);
        return;
    }

    send_message_header(client_socket, 0, MSG_TYPE_COMPILED_FILE_READY);

    FILE *compiled_file = fopen(path_to_output_binary, "rb");
    if (compiled_file == NULL) {
        perror("Error opening compiled file");
        close(client_socket);
        free(buffer);
        free(output_binary);
        return;
    }


    size_t send_buffer_size = INITIAL_BUFFER_SIZE;
    char *send_buffer = (char *)malloc(send_buffer_size);
    if (send_buffer == NULL) {
        perror("Error allocating memory for send buffer");
        free(output_binary);
        free(buffer);
        close(client_socket);
        return;
    }

    size_t bytes_read;
    while ((bytes_read = fread(send_buffer, 1, send_buffer_size, compiled_file)) > 0) {
        size_t bytes_sent = 0;
        while (bytes_sent < bytes_read) {
            ssize_t sent = send(client_socket, send_buffer + bytes_sent, bytes_read - bytes_sent, 0);

            if (sent == -1) {
                perror("Error sending compiled file");
                break;
            }
            bytes_sent += sent;
        }
        if (bytes_sent != bytes_read) {
            break;
        }
    }



    free(send_buffer);
    fclose(compiled_file);
    free(output_binary);
    free(buffer);

//    if (remove(file_path) == 0 && remove(path_to_output_binary) == 0) {
//        printf("Files '%s' and '%s' deleted successfully.\n", file_path, path_to_output_binary);
//    } else {
//        perror("Error deleting file");
//    }

    free(file_path);
    close(client_socket);
}
