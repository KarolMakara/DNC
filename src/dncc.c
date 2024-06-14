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
#include <sys/socket.h>
#include <regex.h>

#include "message.h"
#include "constants.h"
#include "helpers.h"
#include "networking.h"
#include "load_balancer.h"


size_t client_count = 0;

//todo fix when compiling itself, needs to read all args
int recompile_locally(int argc, char **argv, int *exit_code) {

    char *recompile_file = NULL;
    regex_t regex;
    int reti = regcomp(&regex, "\\.o$", REG_EXTENDED);
    if (reti != 0) {
        fprintf(stderr, "Failed to compile regex\n");
        return -1;
    }

    char *output_file = NULL;
    for (int i = 2; i < argc; i++) {
        reti = regexec(&regex, argv[i], 0, NULL, 0);
        if (reti == 0) {
            recompile_file = malloc(strlen(argv[i]) + 2);
            if (recompile_file == NULL) {
                fprintf(stderr, "Memory allocation failed\n");
                return -1;
            }

            strcpy(recompile_file, argv[i]);
            char *extension = strstr(recompile_file, ".o");
            strcpy(extension, ".c");
        }
        if (strstr(argv[i], "-o") != NULL) {
            output_file = strdup(argv[i + 1]);
        }
    }

    char recompile_command[MAX_COMMAND_LENGTH] = "";
    strcat(recompile_command, argv[1]);
    strcat(recompile_command, " -c ");
    strcat(recompile_command, recompile_file);
    strcat(recompile_command, " -o ");
    strcat(recompile_command, output_file);
    *exit_code = execute_command(recompile_command);


    printf("\\033[31m recompile_command: %s\n", recompile_command);

    free(recompile_file);
    free(output_file);
    regfree(&regex);

    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s gcc/g++ <gcc_opts> \n", argv[0]);
        return 1;
    }

    for(int i = 1; i < argc; i++) {
//        printf("%s \n", argv[i]);
    }


    uint16_t file_type = 0;
    if (strstr(argv[1], "gcc") != NULL) {
        file_type = MSG_TYPE_FILE_C;
    } else if (strstr(argv[1], "g++") != NULL) {
        file_type = MSG_TYPE_FILE_CPP;
    }

    char *source_file = NULL;
    char *preprocess_output_file = NULL;
    char *output_binary = NULL;

    int is_output_name = -1;
    int link_only = 0;

//    printf("ARGS:\n");
    for (int i = 2; i < argc; i++) {
//        printf("%s\n", argv[i]);
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            if (output_binary != NULL) {
                free(output_binary);
            }
            output_binary = malloc(strlen(argv[i + 1]) + 1);
            if (output_binary != NULL) {
                strcpy(output_binary, argv[i + 1]);
            } else {
                fprintf(stderr, "Failed to allocate memory for output binary\n");
            }
            is_output_name = 0;
        } else if (strstr(argv[i], "-c") != NULL || strstr(argv[i], "-S") != NULL) {
            link_only = -1;
        } else {
            regex_t regex;
            int reti = regcomp(&regex, "\\.c(pp)?$", REG_EXTENDED);
            if (reti != 0) {
                fprintf(stderr, "Failed to compile regex\n");
            } else {
                reti = regexec(&regex, argv[i], 0, NULL, 0);
                if (reti == 0) {
                    if (source_file != NULL) {
                        free(source_file);
                    }
                    source_file = strdup(argv[i]);
//                    printf("SOURCE FILE %d: %s\n", i, source_file);
                }
                regfree(&regex);
            }
        }
    }


    char command[MAX_COMMAND_LENGTH] = "";
    if (source_file != NULL) {
//        printf("SOURCE FILE: %s\n", source_file);
    }

//    printf("%s\n", output_binary);


    strcat(command, argv[1]);
    strcat(command, " ");


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

    int exit_code = execute_command(command);


    if (exit_code != 0) {
        printf("Exitcode: -1 ");printf("%s\n", command);
        printf("preprocess_output_file %s\n", preprocess_output_file);

        recompile_locally(argc, argv, &exit_code);

        if (exit_code == 0) {
            return 0;
        }



        free(preprocess_output_file);
        free(output_binary);
        free(source_file);
//        recompile_locally();
        return -1;
    }

    if (link_only == 0 || is_output_name == -1) {
        free(preprocess_output_file);
        free(output_binary);
        free(source_file);
        printf("Exitcode: 0 ");printf("%s\n", command);
        return 0;
    }
    free(source_file);


    FILE *file = fopen(preprocess_output_file, "r");
    if (file == NULL) {
        free(output_binary);
        perror("Error opening file");
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    if (file_size < 0) {
        perror("Error getting file size");
        fclose(file);
        free(preprocess_output_file);
        free(output_binary);
        return 1;
    }
    fseek(file, 0, SEEK_SET);

    char *file_contents = (char *)malloc(file_size + 1);
    if (file_contents == NULL) {
        perror("Memory allocation failed");
        fclose(file);
        free(preprocess_output_file);
        free(output_binary);
        return 1;
    }

    fread(file_contents, 1, file_size, file);
    free(preprocess_output_file);
    fclose(file);
    file_contents[file_size] = '\0';
    file_size++;


    char *output_binary_name = extract_binary_name(output_binary);

    int server_count = 0;
    char **server_list = read_server_list(SERVER_LIST_CFG, &server_count);
    if (server_list == NULL) {
        free(file_contents);
        free(output_binary);
        printf("Exitcode: -1 ");printf("DEBUG2 %s\n", command);
        return -1;
    }

    int client_socket;
    int connection = -1;

    size_t server_index;

    struct sockaddr_in server_addr;
    char ip_addr[16];
    uint16_t port;

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 500;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket == -1) {
        perror("Socket creation failed");
        free(file_contents);
        free_arr(server_list, server_count);
        free(output_binary);
        printf("Exitcode: -1 ");printf("DEBUG3 %s\n", command);
        return -1;
    }

    int tried_ips = 1;

    while (tried_ips <= 999) {

        server_index = get_first_digit_milliseconds() % server_count;

        if (sscanf(server_list[server_index], "%[^:]:%hu", ip_addr, &port) != 2) {
            fprintf(stderr, "Invalid server address format: %s\n", server_list[server_index]);
            continue;
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, ip_addr, &server_addr.sin_addr) <= 0) {
            fprintf(stderr, "Invalid IP address: %s\n", ip_addr);
            tried_ips++;
            continue;
        }
//        else {
//            printf("Trying ip... (%s)\n", ip_addr);
//        }

//        connection = connect_with_timeout(client_socket, server_addr, timeout);
        connection = connect(client_socket, (struct sockaddr *) &server_addr, sizeof(server_addr));
        if (connection == 0) {
            remove_socket_timout(client_socket); // remove timeout if connected to receive long time compiled files
            int server_ready = is_server_ready(client_socket);
            if (server_ready == 0) {
                if (send_file_to_server(client_socket, file_contents, file_size, output_binary_name, file_type) == 0) {
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

        sleep(RETRY_DELAY);
    }

    if (connection != 0) {
        free(file_contents);
        free(output_binary);
        free_arr(server_list, server_count);
        printf("Exitcode: -1 ");printf("DEBUG4 %s\n", command);
        return -1;
    }

    int error_no = 0;
    MessageHeader header = receive_message_header(client_socket, &error_no);

    if (error_no == -1) {
        close(client_socket);
        free(file_contents);
        free(output_binary);
        free_arr(server_list, server_count);
        recompile_locally(argc, argv, &exit_code);
        if (exit_code == 0) {
            return 0;
        }
        return -1;
    }

    if (header.type != MSG_TYPE_COMPILED_FILE_READY) {
        fprintf(stderr, "Unexpected message type: %u\n", header.type);
        close(client_socket);
    }
    receive_compiled_file(client_socket, output_binary);


    close(client_socket);
    free(output_binary);
    free(file_contents);

    free_arr(server_list, server_count);
    printf("Exitcode: 0\n");
    return 0;
}
