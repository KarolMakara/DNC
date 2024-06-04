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
#include <sys/socket.h>

#include "message.h"
#include "constants.h"
#include "networking.h"


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

void free_arr(char **server_list, int server_count) {
    for (int i = 0; i < server_count; i++) {
        if (server_list && server_list[i] != NULL) {
            free(server_list[i]);
        }
    }
    free(server_list);
}

char **read_server_list(const char *filename, int *server_count) {
    char **server_list = NULL;
    *server_count = 0;
    printf("DEBUG filename %s\n", filename);
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
            free_arr(server_list, *server_count);
            return NULL;
        }
        server_list = new_server_list;

        server_list[*server_count] = strdup(line);
        if (server_list[*server_count] == NULL) {
            perror("Error allocating memory for server address");
            fclose(server_file);
            free_arr(server_list, *server_count);
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


    printf("============= Running with args: =============\n");
    for(int i = 1; i < argc; i++) {
        printf("%s \n", argv[i]);
    }
    printf("============= End running args =============\n");

    char *source_file = NULL;
    char *preprocess_output_file = NULL;
    char *output_binary = NULL;

    int is_output_name = -1;
    int link_only = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_binary = strdup(argv[i + 1]);
            is_output_name = 0;
        } else if (strstr(argv[i], "-c") != NULL || strstr(argv[i], "-S") != NULL) {
            link_only = -1;
        } else if (strstr(argv[i], ".c") != NULL) {
            source_file = strdup(argv[i]);
        }
    }


    char command[MAX_COMMAND_LENGTH] = "";
    if (source_file != NULL) {
        printf("============= source_file start =============\n");
        printf("%s\n", source_file);
        printf("============= source_file end =============\n");
    } else {
        printf("No source file specified. Probaby linking! \n");
    }


    printf("============= output_binary start =============\n");
    printf("%s\n", output_binary);
    printf("============= output_binary end =============\n");


    strcat(command, argv[1]);
    strcat(command, " ");

    printf("============= Linking start =============\n");
    printf("%d\n", link_only);
    printf("============= Linking end =============\n");


    printf("============= Pre Command start =============\n");
    printf("%s\n", command);
    printf("============= Pre Command end =============\n");

    if (link_only == 0 || is_output_name == -1) {
        for (int i = 2; i < argc; i++) {
            strcat(command, argv[i]);
            strcat(command, " ");
        }
    } else {
        strcat(command, "-E ");
        if (output_binary != NULL) {
            preprocess_output_file = malloc(strlen(source_file) + 3);
            strcpy(preprocess_output_file, source_file);
            strcat(preprocess_output_file, ".i");
        }


        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], output_binary) == 0) {
                strcat(command, preprocess_output_file);
            } else if (strstr(argv[i], "-c") == NULL) {
                strcat(command, argv[i]);
            }
            strcat(command, " ");
        }
    }

    printf("============= Command start =============\n");
    printf("%s\n", command);
    printf("============= Command end =============\n");



    int status = system(command);
    printf("Running command:\n %s\n", command);
    if (status == -1) {
        perror("Error in executing command");
        free(preprocess_output_file);
        return 1;
    } else if (link_only == 0 || is_output_name == -1) {
        free(preprocess_output_file);
        return 0;
    }
    free(source_file);


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
        free(preprocess_output_file);
        return 1;
    }
    fseek(file, 0, SEEK_SET);

    char *file_contents = (char *)malloc(file_size + 1);
    if (file_contents == NULL) {
        perror("Memory allocation failed");
        fclose(file);
        free(preprocess_output_file);
        return 1;
    }

    fread(file_contents, 1, file_size, file);
    free(preprocess_output_file);
    fclose(file);
    file_contents[file_size] = '\0';
    file_size++;


    char *output_binary_name = extractBinaryName(output_binary);

    int server_count = 0;
    char **server_list = read_server_list("/home/kmakara/DNCC/server_list.cfg", &server_count);
    if (server_list == NULL) {
        free(file_contents);
        return 1;
    }

    shuffle(server_list, server_count);

    int client_socket = -1;
    int server_index = 0;

    struct sockaddr_in server_addr;
    char ip_addr[16];
    uint16_t port;

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 500;

    int connection = -1;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket == -1) {
        perror("Socket creation failed");
        free(file_contents);
        free_arr(server_list, server_count);
        return 1;
    }

    int tried_ips = 1;

    while (tried_ips <= server_count) {

        if (sscanf(server_list[server_index], "%[^:]:%hu", ip_addr, &port) != 2) {
            fprintf(stderr, "Invalid server address format: %s\n", server_list[server_index]);
            continue;
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, ip_addr, &server_addr.sin_addr) <= 0) {
            fprintf(stderr, "Invalid IP address: %s\n", ip_addr);
            server_index = (server_index + 1) % server_count;
            tried_ips++;
            continue;
        } else {
            printf("Trying ip... (%s)\n", ip_addr);
        }

        connection = connect_with_timeout(client_socket, server_addr, timeout);

        if (connection == 0) {
            remove_socket_timout(client_socket); // remove timeout if connected to receive long time compiled files
            int server_ready = isServerReady(client_socket);
            if (server_ready == 0) {
                if (sendFileToServer(client_socket, file_contents, file_size, output_binary_name) == 0) {
                    break;
                }
            }
        }


        if (connection != 0) {
            tried_ips++;
            close(client_socket);
            client_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (client_socket == -1) {
                perror("Socket creation failed");
                continue;
            }
        }


        server_index = (server_index + 1) % server_count;
        sleep(RETRY_DELAY);
    }

    if (connection != 0) {
        printf("Could not connect to any server");
        free(file_contents);
        free(output_binary);
        free_arr(server_list, server_count);
        return -1;
    }

    int error_no = 0;
    MessageHeader header = receive_message_header(client_socket, &error_no);

    if (error_no == -1) {
        close(client_socket);
        free(file_contents);
        free(output_binary);
        free_arr(server_list, server_count);
        return -1;
    }

    if (header.type != MSG_TYPE_COMPILED_FILE_READY) {
        fprintf(stderr, "Unexpected message type: %u\n", header.type);
        close(client_socket);
    }
    receiveCompiledFile(client_socket, output_binary);

    close(client_socket);
    free(output_binary);
    free(file_contents);
    free_arr(server_list, server_count);
    return 0;
}
