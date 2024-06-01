#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/time.h>


#include "message.h"


#define PORT 8080
#define INITIAL_BUFFER_SIZE 16384 //todo fix pointer realloc and add to server
#define MAX_COMMAND_LENGTH 1024
#define MAX_BINARY_NAME_LENGTH 128
#define MAX_RETRIES 5
#define RETRY_DELAY 1
#define MAX_SERVERS 10



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


int isServerReady(int client_socket) {
    MessageHeader header;
    memset(&header, 0, sizeof(MessageHeader));
    header.type = MSG_TYPE_CHECK_READINESS;
    header.length = 12333333;
    send_message_header(client_socket, header);
    printf("SEND header.type: %hhu\n header.length: %u\n", header.type, header.length);
    memset(&header, 0, sizeof(MessageHeader));

    ssize_t bytes_received = recv(client_socket, &header, sizeof(MessageHeader), 0);
    printf("RECV header.type: %hhu\n header.length: %u\n", header.type, header.length);
    if (bytes_received == -1) {
        perror("Receive failed");
        return 0;
    }

    if (header.type == MSG_TYPE_SERVER_READY) {
        memset(&header, 0, sizeof(MessageHeader));
        return 1;
    } else if (header.type == MSG_TYPE_SERVER_NOT_READY) {
        printf("Server is not ready for compilation.\n");
        return 0;
    } else {
        fprintf(stderr, "Unexpected message type: %u\n", header.type);
        return 0;
    }
}

int sendFileToServer(int client_socket, const char *data, size_t data_length, const char *output_binary) {

    if (!isServerReady(client_socket)) {
        fprintf(stderr, "Server is not ready for compilation.\n");
        return 1;
    } else {
        printf("Server is ready for compilation.\n");
        
    }
    MessageHeader header;
    memset(&header, 0, sizeof(MessageHeader));
    header.type = MSG_TYPE_FILE_DATA;
    header.length = (uint32_t)data_length;

    send_message_header(client_socket, header);
    printf("SENDFILE1header.type: %hhu\n header.length: %u\n", header.type, header.length);
    //not accessible
    if (send_message(client_socket, data, data_length)) {
        fprintf(stderr, "Failed to send file data.\n");
        return 1;
    }

    receiveACK(client_socket);

    size_t output_binary_length = strlen(output_binary);

    memset(&header, 0, sizeof(MessageHeader));
    header.type = MSG_TYPE_OUTPUT_BINARY;
    header.length = (uint32_t)output_binary_length;

    send_message_header(client_socket, header);
    printf("SENDFILE2header.type: %hhu\n header.length: %u\n", header.type, header.length);

    if (send_message(client_socket, output_binary, output_binary_length)) {
        fprintf(stderr, "Failed to send output binary name.\n");
        return 1;
    }

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

    FILE *output_file = fopen(output_filename, "wb");
    if (output_file == NULL) {
        perror("Error opening file for writing");
        free(buffer);
        return;
    }

    // Set the socket to non-blocking mode
    int flags = fcntl(client_socket, F_GETFL, 0);
    fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);

    fd_set read_fds;
    struct timeval timeout;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);

        timeout.tv_sec = 5; // Set the desired timeout value in seconds
        timeout.tv_usec = 0;

        int select_result = select(client_socket + 1, &read_fds, NULL, NULL, &timeout);

        if (select_result == -1) {
            perror("Error in select");
            break;
        } else if (select_result == 0) {
            printf("Timeout occurred while waiting for data.\n");
            break;
        } else {
            bytes_received = recv(client_socket, buffer, buffer_size, 0);
            if (bytes_received == 0) {
                // Server closed the connection
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
    }

    if (total_received > 0) {
        printf("Compiled file received from server and saved as %s.\n", output_filename);
    } else {
        printf("No data received, not writing to file.\n");
    }

    fclose(output_file);
    free(buffer);

    // Set the socket back to blocking mode
    fcntl(client_socket, F_SETFL, flags);
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
            printf("DEBUG 4.1\n");
            output_binary = strdup(argv[i + 1]);
            printf("DEBUG 4.2\n");
            is_output_name = true;
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            printf("DEBUG 4.1\n");
            source_file = strdup(argv[i + 1]);
            printf("DEBUG 4.2\n");
            link_only = false;
        }
        printf(" %s", argv[i]);
    }
    printf("\nlink only %d\n", link_only);
    printf("is_output_name %d\n", is_output_name);
    printf("output_binary: %s\n", output_binary);


    char command[MAX_COMMAND_LENGTH] = "";

    if (link_only || !is_output_name) {
        for (int i = 1; i < argc; i++) {
            strcat(command, argv[i]);
            strcat(command, " ");
        }
    } else {
        if (output_binary != NULL) {
            preprocess_output_file = malloc(strlen(source_file) + 3);
            strcpy(preprocess_output_file, source_file);
            strcat(preprocess_output_file, ".i");
        }

        free(source_file);


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
    if (file_size < 0) {
        perror("Error getting file size");
        fclose(file);
        return 1;
    }
    fseek(file, 0, SEEK_SET);
    printf("File size: %ld\n", file_size); // Print file size for debugging purposes

    char *file_contents = (char *)malloc(file_size + 1);
    if (file_contents == NULL) {
        perror("Memory allocation failed");
        fclose(file);
        return 1;
    }

    fread(file_contents, 1, file_size, file);
    free(preprocess_output_file);
    fclose(file);
    file_contents[file_size] = '\0';
    file_size++;


    char *output_binary_name = extractBinaryName(output_binary);

    free(output_binary);

    char server_list[MAX_SERVERS][32];
    int server_count = 0;
    FILE *server_file = fopen("server_list.cfg", "r");
    if (server_file == NULL) {
        perror("Error opening server list file");
        return 1;
    }

    char line[32];
    while (fgets(line, sizeof(line), server_file) != NULL && server_count < MAX_SERVERS) {
        char *newline = strchr(line, '\n');
        if (newline != NULL) {
            *newline = '\0';
        }
        strncpy(server_list[server_count], line, sizeof(server_list[server_count]) - 1);
        server_list[server_count][sizeof(server_list[server_count]) - 1] = '\0';
        server_count++;
    }
    fclose(server_file);


    int client_socket = -1;
    int retry_count = 0;
    int server_index = 0;

    while (retry_count < MAX_RETRIES) {
        struct sockaddr_in server_addr;
        char ip_addr[16];
        uint16_t port;

        if (sscanf(server_list[server_index], "%[^:]:%hu", ip_addr, &port) != 2) {
            fprintf(stderr, "Invalid server address format: %s\n", server_list[server_index]);
            return 1;
        }

        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket == -1) {
            perror("Socket creation failed");
            return 1;
        }

        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, ip_addr, &server_addr.sin_addr) <= 0) {
            fprintf(stderr, "Invalid IP address: %s\n", ip_addr);
            close(client_socket);
            server_index = (server_index + 1) % server_count;
            retry_count++;
            continue;
        }

        printf("Attempting to connect to server %s:%d (try %d)...\n", ip_addr, port, retry_count + 1);
        if (connect(client_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) == 0) {
            printf("Connected to server successfully.\n");
            if (
                    sendFileToServer(client_socket, file_contents, file_size, output_binary_name) != 0
            ) {
                close(client_socket);
                server_index = (server_index + 1) % server_count;
                retry_count = 0;
                printf("DEBUG: !isServerReady(client_socket)");
                continue;
            }
            break;
        } else {
            perror("Connection to server failed");
            close(client_socket);
            retry_count++;
            sleep(RETRY_DELAY);
        }


    }
    free(file_contents);


    MessageHeader header;
    ssize_t bytes_received = recv(client_socket, &header, sizeof(MessageHeader), 0);
    if (bytes_received <= 0) {
        perror("Error receiving compiled file ready message");
        close(client_socket);
//        continue;
    }

    if (header.type != MSG_TYPE_COMPILED_FILE_READY) {
        fprintf(stderr, "Unexpected message type: %u\n", header.type);
        close(client_socket);
//        continue;
    }
    printf("receiveCompiledFile");
    receiveCompiledFile(client_socket, output_binary_name);

    close(client_socket);
    return 0;
}
