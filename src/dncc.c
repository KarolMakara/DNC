#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <stdbool.h>

#include "message.h"


#define PORT 8080
#define INITIAL_BUFFER_SIZE 16384 //todo fix pointer realloc and add to server
#define MAX_COMMAND_LENGTH 1024
#define MAX_BINARY_NAME_LENGTH 128
#define MAX_RETRIES 5
#define RETRY_DELAY 1

int is_binary(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening file");
        return -1;
    }

    int isBinary = 0;
    unsigned char buffer[1024];
    size_t bytesRead;

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        for (size_t i = 0; i < bytesRead; ++i) {
            if (buffer[i] == 0x00) {
                isBinary = 1;
                break;
            }
        }
        if (isBinary) {
            break;
        }
    }

    fclose(file);
    return isBinary;
}

void send_message_header(int client_socket, MessageHeader header) {

    if (send(client_socket, &header, sizeof(header), 0) < 0) {
        perror("Sending header failed");
        close(client_socket);
    }
}

int send_message(int client_socket, const char* data, size_t data_size) {
    if (send(client_socket, data, data_size, 0) < 0) {
        perror("Sending message failed");
        close(client_socket);
        return 1;
    }
    return 0;
}

void receiveACK(int socket_fd) {
    MessageHeader header;
    ssize_t bytes_received = recv(socket_fd, &header, sizeof(MessageHeader), 0);
    if (bytes_received == -1) {
        perror("Receive failed");
        exit(EXIT_FAILURE);
    }

    if (header.type != MSG_TYPE_ACK) {
        fprintf(stderr, "Expected ACK, received message type: %u\n", header.type);
        exit(EXIT_FAILURE);
    }

    if (header.length != 0) {
        fprintf(stderr, "Invalid ACK length: %u\n", header.length);
        exit(EXIT_FAILURE);
    }

    printf("Received ACK from server\n");
}


int sendFileToServer(int client_socket, const char *data, size_t data_length, const char *output_binary) {

    MessageHeader header = {};

    memset(&header, 0, sizeof(header));
    header.type = MSG_TYPE_FILE_DATA;
    header.length = (uint32_t)data_length;

    send_message_header(client_socket, header);
    send_message(client_socket, data, data_length);

    receiveACK(client_socket);


    size_t output_binary_length = strlen(output_binary);

    memset(&header, 0, sizeof(header));
    header.type = MSG_TYPE_OUTPUT_BINARY;
    header.length = (uint32_t)output_binary_length;

    send_message_header(client_socket, header);
    send_message(client_socket, output_binary, output_binary_length);


    return 0;
}


void receiveCompiledFile(int client_socket, const char *output_filename) {
    char *buffer = malloc(INITIAL_BUFFER_SIZE);
    if (buffer == NULL) {
        perror("Error allocating memory");
        return;
    }

    size_t buffer_size = INITIAL_BUFFER_SIZE;
    size_t bytes_received;
    size_t total_received = 0;

    FILE *output_file = NULL;
    int data_received = 0;

    while ((bytes_received = recv(client_socket, buffer + total_received, buffer_size - total_received, 0)) > 0) {
        total_received += bytes_received;
        printf("total_received %zu, buffer_size %zu\n", total_received, buffer_size);
        if (total_received == buffer_size) {
            buffer_size *= 2;
            char *new_buffer = realloc(buffer, buffer_size);
            if (new_buffer == NULL) {
                perror("Error reallocating memory");
                printf("buffer_size %zu\n", buffer_size);
                free(buffer);
                return;
            }
            buffer = new_buffer;
        }

        if (!data_received) {
            output_file = fopen(output_filename, "wb");
            if (output_file == NULL) {
                perror("Error opening file for writing");
                free(buffer);
                return;
            }
            data_received = 1;
        }
        fwrite(buffer, 1, total_received, output_file);
    }

    if (data_received && output_file != NULL) {
        printf("Compiled file received from server and saved as %s.\n", output_filename);
        fclose(output_file);
    } else {
        printf("No data received, not writing to file.\n");
    }

    free(buffer);

//    if (chmod(output_filename, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
//        perror("Error setting executable permissions");
//    } else {
//        printf("Executable permissions set on %s.\n", output_filename);
//    }
}


char* extractBinaryName(const char* path) {
    static char binaryName[MAX_BINARY_NAME_LENGTH];
    const char* lastSeparator = strrchr(path, '/');
    if (lastSeparator == NULL) {
        strncpy(binaryName, path, MAX_BINARY_NAME_LENGTH - 1);
        binaryName[MAX_BINARY_NAME_LENGTH - 1] = '\0';
    } else {
        strncpy(binaryName, lastSeparator + 1, MAX_BINARY_NAME_LENGTH - 1);
        binaryName[MAX_BINARY_NAME_LENGTH - 1] = '\0';
    }
    return binaryName;
}
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s gcc <gcc_opts> \n", argv[0]);
        return 1;
    }

    char *source_file = NULL;
    char *preprocess_output_file = NULL;
    char *output_binary = NULL;

    bool is_output_name = false;
    bool link_only = true;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_binary = strdup(argv[i + 1]);
            is_output_name = true;
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            source_file = strdup(argv[i + 1]);
            link_only = false;
        }
        printf(" %s", argv[i]);
    }
    printf("\nlink only %d\n", link_only);
    printf("is_output_name %d\n", is_output_name);

    char command[MAX_COMMAND_LENGTH] = "";

    if (link_only || !is_output_name) {
        for (int i = 1; i < argc; i++) {
            strcat(command, argv[i]);
            strcat(command, " ");
        }
    } else {
        if (output_binary) {
            preprocess_output_file = malloc(strlen(source_file) + 3);
            strcpy(preprocess_output_file, source_file);
            strcat(preprocess_output_file, ".i");
        }


        strcat(command, "gcc ");
        strcat(command, "-E ");


        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], output_binary) == 0) {
                strcat(command, preprocess_output_file);
            } else {
                strcat(command, argv[i]);
            }
            strcat(command, " ");
        }
    }



    printf("Running preprocess command: %s\n", command);
    int status = system(command);
    if (status == -1) {
        perror("Error in executing command");
        return 1;
    } else if (link_only || !is_output_name) {
        return 0;
    }


    FILE *file = fopen(preprocess_output_file, "r");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *file_contents = (char *)malloc(file_size + 1);
    if (file_contents == NULL) {
        perror("Memory allocation failed");
        fclose(file);
        return 1;
    }

    fread(file_contents, 1, file_size, file);
    fclose(file);
    file_contents[file_size] = '\0';


    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int retry_count = 0;
    while (retry_count < MAX_RETRIES) {
        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket == -1) {
            perror("Socket creation failed");
            return 1;
        }

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        printf("Attempting to connect to server (try %d)...\n", retry_count + 1);
        if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0) {
            printf("Connected to server successfully.\n");
            break;
        } else {
            perror("Connection to server failed");
            close(client_socket);
            retry_count++;
            sleep(RETRY_DELAY);
        }
    }

    char *output_binary_name = extractBinaryName(output_binary);

    sendFileToServer(client_socket, file_contents, file_size, output_binary_name);

    receiveCompiledFile(client_socket, output_binary);

    free(file_contents);
    free(output_binary);
    free(preprocess_output_file);
    return 0;
}
