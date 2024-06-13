#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include "message.h"
#include "constants.h"
#include "file_operations.h"


int client_count = 0;

void sha256(const char *str, char *hash) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(mdctx, str, strlen(str));
    EVP_DigestFinal_ex(mdctx, digest, NULL);
    EVP_MD_CTX_free(mdctx);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(hash + (i * 2), "%02x", digest[i]);
    }
    hash[64] = 0;
}


int directoryExists(const char *path) {
    struct stat info;
    if (stat(path, &info) != 0) {
        return 0;
    }
    return (info.st_mode & S_IFDIR) != 0;
}

int createDirectory(const char *path) {
    if (!directoryExists(path)) {
        if (mkdir(path, 0777) == -1) {
            perror("Error creating directory");
            return 1;
        }
    } else {
    }
    return 0;
}

char* receiveFile(int client_socket, size_t data_length) {
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
    snprintf(worker_dir, strlen(SERVER_DIRECTORY) + 2 + 1, "%s/%d", SERVER_DIRECTORY, client_count);

    createDirectory(worker_dir);


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

void handleClient(int client_socket) {

    char* file_path = NULL;


    int error_no = 0;
    MessageHeader header = receive_message_header(client_socket, &error_no);

    if (error_no == -1) {
        return;
    }


    if (header.type == MSG_TYPE_FILE_DATA) {
        file_path = receiveFile(client_socket, header.length);
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

    printf("FILETYPEEEEE: %d\n", file_type);

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
    snprintf(path_to_output_binary, MAX_BINARY_NAME_LENGTH, "%s/%d/%s", SERVER_DIRECTORY, client_count, output_binary);


    char command[MAX_COMMAND_LENGTH];

    char command_holder[16];


    if (file_type == FILE_TYPE_C) {
        strcpy(command_holder, "gcc -c %s %s %s");
    } else if (file_type == FILE_TYPE_CPP) {
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
    printf("Running command:\n %s\n", command);
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

//    if (remove(file_path) == 0 && remove(path_to_output_binary) == 0) {
//    } else {
//        perror("Error deleting file");
//    }

    free(send_buffer);
    fclose(compiled_file);
    free(output_binary);
    free(buffer);


    free(file_path);
    close(client_socket);
}

void sigchld_handler(__attribute__((unused)) int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        client_count--;
    }
}

int main() {
    createDirectory(SERVER_DIRECTORY);

    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    signal(SIGCHLD, sigchld_handler);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));


    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    while (1) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        } else {
        }

        int error_no = 0;
        MessageHeader header = receive_message_header(client_socket, &error_no);

        if (error_no == -1) {
            return -1;
        }

        if (header.type == MSG_TYPE_CHECK_READINESS) {

            if (client_count >= MAX_CLIENTS) {
                send_message_header(client_socket, 0, MSG_TYPE_SERVER_NOT_READY);
                continue;
            }

            send_message_header(client_socket, 0, MSG_TYPE_SERVER_READY);
            client_count++;
        } else {
            perror("Unexpected message type");
            continue;
        }

        pid_t child_pid = fork();
        if (child_pid == -1) {
            perror("Fork failed");
            close(client_socket);
            continue;
        } else if (child_pid == 0) {
            handleClient(client_socket);
            client_count--;
            exit(EXIT_SUCCESS);
        } else {
            close(client_socket);
        }
    }
}