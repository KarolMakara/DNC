#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>

#include "message.h"
#include "constants.h"


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

char **read_server_list(const char *filename, int *server_count) {
    char **server_list = NULL;
    *server_count = 0;
    FILE *server_file = fopen(filename, "r");
    if (server_file == NULL) {
        perror("Error opening server list file");
        return NULL;
    }

    char line[32];
    while (fgets(line, sizeof(line), server_file) != NULL) {
        char *newline = strchr(line, '\n');
        if (newline != NULL) {
            *newline = '\0';
        }

        char **new_server_list = realloc(server_list, (*server_count + 1) * sizeof(char *));
        if (new_server_list == NULL) {
            perror("Error allocating memory for server list");
            fclose(server_file);
            for (int i = 0; i < *server_count; i++) {
                free(server_list[i]);
            }
            free(server_list);
            return NULL;
        }
        server_list = new_server_list;

        server_list[*server_count] = strdup(line);
        if (server_list[*server_count] == NULL) {
            perror("Error allocating memory for server address");
            fclose(server_file);
            for (int i = 0; i < *server_count; i++) {
                free(server_list[i]);
            }
            free(server_list);
            return NULL;
        }

        (*server_count)++;
    }

    fclose(server_file);
    return server_list;
}

void shuffle(char **server_list, int server_count) {
    srand(time(NULL));
    for (int i = server_count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        char *temp = server_list[i];
        server_list[i] = server_list[j];
        server_list[j] = temp;
    }
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s gcc/g++ <gcc_opts> \n", argv[0]);
        return 1;
    }

    char *source_file = NULL;
    char *preprocess_output_file = NULL;
    char *output_binary = NULL;

    int is_output_name = -1;
    int link_only = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_binary = strdup(argv[i + 1]);
            is_output_name = 0;
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            source_file = strdup(argv[i + 1]);
            link_only = -1;
        }
    }


    char command[MAX_COMMAND_LENGTH] = "";

    if (link_only == 0 || is_output_name == -1) {
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


        strcat(command, argv[1]);
        strcat(command, " -E ");


        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], output_binary) == 0) {
                strcat(command, preprocess_output_file);
            } else if (strstr(argv[i], "-c") == NULL) {
                strcat(command, argv[i]);
            }
            strcat(command, " ");
        }
    }



    int status = system(command);
    printf("Running command:\n %s\n", command);
    if (status == -1) {
        perror("Error in executing command");
        return 1;
    } else if (link_only == 0 || is_output_name == -1) {
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

    int server_count = 0;
    char **server_list = read_server_list("server_list.cfg", &server_count);
    if (server_list == NULL) {
        return 1;
    }

    shuffle(server_list, server_count);

    for (int i = 0; i < server_count; i++) {
        printf("%s\n", server_list[i]);
    }

    int client_socket = -1;
    int retry_count = 0;
    int server_index = 0;

    struct sockaddr_in server_addr;
    char ip_addr[16];
    uint16_t port;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket == -1) {
        perror("Socket creation failed");
        return 1;
    }

    int tried_ips = 0;

    while (retry_count < MAX_RETRIES && tried_ips < server_count) {
        if (sscanf(server_list[server_index], "%[^:]:%hu", ip_addr, &port) != 2) {
            fprintf(stderr, "Invalid server address format: %s\n", server_list[server_index]);
            close(client_socket);
            return 1;
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, ip_addr, &server_addr.sin_addr) <= 0) {
            fprintf(stderr, "Invalid IP address: %s\n", ip_addr);
            server_index = (server_index + 1) % server_count;
            tried_ips++;
            continue;
        }

        if (connect(client_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) == 0) {
            int server_ready = isServerReady(client_socket);
            if (server_ready == 0) {
                if (sendFileToServer(client_socket, file_contents, file_size, output_binary_name) == 0) {
                    break;
                }
            } else {
                close(client_socket);
                client_socket = socket(AF_INET, SOCK_STREAM, 0);
                if (client_socket == -1) {
                    perror("Socket creation failed");
                    return 1;
                }
            }
        }

        server_index = (server_index + 1) % server_count;
        retry_count++;
        sleep(RETRY_DELAY);
    }

    int error_no = 0;
    MessageHeader header = receive_message_header(client_socket, &error_no);

    if (error_no == -1) {
        close(client_socket);
        return -1;
    }

    if (header.type != MSG_TYPE_COMPILED_FILE_READY) {
        fprintf(stderr, "Unexpected message type: %u\n", header.type);
        close(client_socket);
    }
    receiveCompiledFile(client_socket, output_binary_name);

    close(client_socket);
    return 0;
}
