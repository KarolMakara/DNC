#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include "message.h"


#define PORT 8080
#define MAX_BUFFER_SIZE 32768
#define SERVER_DIRECTORY "/tmp/compilation_server"


char* createHashDirPath(const char *file_hash) {
    if (file_hash == NULL) {
        fprintf(stderr, "file_hash is NULL\n");
        return NULL;
    }

    // Determine the length of the concatenated string
    size_t dir_len = strlen(SERVER_DIRECTORY);
    size_t hash_len = strlen(file_hash);
    size_t total_len = dir_len + hash_len + 1; // +1 for the null terminator

    // Allocate memory for the concatenated string
    char *hash_dir = (char *)malloc(total_len * sizeof(char));
    if (hash_dir == NULL) {
        perror("Unable to allocate memory");
        exit(EXIT_FAILURE);
    }

    // Copy SERVER_DIRECTORY to the allocated memory
    strcpy(hash_dir, SERVER_DIRECTORY);

    // Concatenate file_hash to the buffer
    strcat(hash_dir, file_hash);

    return hash_dir; // Return the dynamically allocated string
}


char* extractOutputPath(const char* str) {
    const char* delim = " ";
    const char* target = "-o";
    char* token;
    char* saveptr;

    token = strtok_r((char*)str, delim, &saveptr);
    while (token != NULL) {
        if (strcmp(token, target) == 0) {
            token = strtok_r(NULL, delim, &saveptr); // Get the next token after "-o"
            if (token != NULL) {
                return token;
            } else {
                return NULL; // If "-o" is the last token, return NULL
            }
        }
        token = strtok_r(NULL, delim, &saveptr);
    }
    return NULL; // Return NULL if not found
}

char* extractFilename(const char *original) {
    char *flag_position = strstr(original, "-o");

    if (flag_position != NULL) {
        // Calculate the position of the filename after '-o' and store it
        char *filename_position = flag_position + 2; // +2 to move past '-o'

        // Skip any leading spaces
        while (*filename_position == ' ') {
            filename_position++;
        }

        // Find the end of the filename
        char *filename_end = strchr(filename_position, ' ');
        if (filename_end == NULL) {
            // If no space is found, the filename goes to the end of the string
            filename_end = filename_position + strlen(filename_position);
        }

        // Calculate the length of the filename
        size_t filename_length = filename_end - filename_position;

        // Allocate memory for the filename
        char *filename = (char*)malloc((filename_length + 1) * sizeof(char)); // +1 for null terminator

        // Copy the filename to the allocated memory
        strncpy(filename, filename_position, filename_length);
        filename[filename_length] = '\0';

        return filename;
    } else {
        //printf("No '-o' flag found.\n");
        return NULL;
    }
}

void prependPathToFolder(char *original, const char *prefix) {
    char *filename = extractFilename(original);
    if (filename != NULL) {
        size_t prefix_length = strlen(prefix);
        size_t filename_length = strlen(filename);
        size_t original_length = strlen(original);

        // Calculate the required buffer size for the new string
        size_t new_length = prefix_length + filename_length + original_length - filename_length + 1;

        // Allocate a temporary buffer
        char *temp_buffer = (char *)malloc(new_length * sizeof(char));
        if (temp_buffer == NULL) {
            perror("Unable to allocate memory");
            free(filename);
            return;
        }

        // Construct the new string in the temporary buffer
        strcpy(temp_buffer, prefix);
        strcat(temp_buffer, filename);
        strcat(temp_buffer, original + filename_length + prefix_length);

        // Copy the new string back to the original buffer
        strcpy(original, temp_buffer);

        // Free the temporary buffer and the extracted filename
        free(temp_buffer);
        free(filename);
    } else {
        printf("No filename found.\n");
    }
}


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
        printf("Directory created successfully: %s\n", path);
    } else {
        printf("Directory already exists: %s\n", path);
    }
    return 0;
}

char* extractDirectoryFromPath(const char *path) {
    // Find the last occurrence of the directory separator '/'
    const char *last_separator = strrchr(path, '/');

    if (last_separator != NULL) {
        // Calculate the length of the directory path
        size_t directory_length = last_separator - path + 1;

        // Allocate memory for the directory path
        char *directory = (char*)malloc((directory_length + 1) * sizeof(char)); // +1 for null terminator

        // Copy the directory path to the allocated memory
        strncpy(directory, path, directory_length);
        directory[directory_length] = '\0';

        return directory;
    } else {
        printf("No directory separator found in the path.\n");
        return NULL;
    }
}


// Function to receive file data from the client and save it
char* receiveFile(int client_socket) {
    MessageHeader header;
    ssize_t valread;
    size_t buffer_size = MAX_BUFFER_SIZE;
    char *buffer = (char *)malloc(buffer_size);
    if (buffer == NULL) {
        perror("Error allocating memory for buffer");
        return NULL;
    }

    // Receive message header
    valread = read(client_socket, &header, sizeof(header));
    if (valread <= 0) {
        perror("Error reading message header");
        free(buffer);
        return NULL;
    }

    if (header.type == MSG_TYPE_FILE_DATA) {
        size_t data_length = header.length;

        // Receive file data
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

            // Extend the buffer size if necessary
            if (bytes_read == buffer_size) {
                buffer_size *= 2;
                char *new_buffer = (char *)realloc(buffer, buffer_size);
                if (new_buffer == NULL) {
                    perror("Error reallocating memory for buffer");
                    free(file_data);
                    free(buffer);
                    return NULL;
                }
                buffer = new_buffer;
            }
        }
        file_data[data_length] = '\0';

        char file_hash[65]; // SHA-256 hash is 64 characters + null terminator
        sha256(file_data, file_hash);

        createDirectory(SERVER_DIRECTORY);

        // Dynamically allocate memory for file path
        char *file_path = (char *)malloc(strlen(SERVER_DIRECTORY) + strlen(file_hash) + 4 + 1); // +4 for ".c" and +1 for null terminator
        if (file_path == NULL) {
            perror("Error allocating memory for file path");
            free(file_data);
            return NULL;
        }
        snprintf(file_path, strlen(SERVER_DIRECTORY) + strlen(file_hash) + 4 + 1, "%s/%s.c", SERVER_DIRECTORY, file_hash);

        FILE *file = fopen(file_path, "w");
        if (file == NULL) {
            perror("Error opening file for writing");
            free(file_data);
            free(file_path);
            return NULL;
        }
        fputs(file_data, file);
        fclose(file);

        // Free allocated memory
        free(file_data);

        free(buffer);
        return file_path;
    } else {
        perror("Unexpected message type");
        return NULL;
    }
}
void sendACK(int client_socket) {
    MessageHeader header = { MSG_TYPE_ACK, 0 };
    ssize_t bytes_sent = send(client_socket, &header, sizeof(MessageHeader), 0);
    if (bytes_sent == -1) {
        perror("Send ACK failed");
        exit(EXIT_FAILURE);
    }
}

void handleClient(int client_socket) {
    // Receive file from client
    char* file_path = receiveFile(client_socket);
    if (file_path == NULL) {
        close(client_socket);
        free(file_path);
        return;
    } else {
        printf("File path: %s\n", file_path);
    }

    sendACK(client_socket);

    MessageHeader header;
    char buffer[MAX_BUFFER_SIZE];
    size_t bytes_received = 0;

    while (bytes_received < sizeof(header)) {
        ssize_t valread = read(client_socket, ((char *)&header) + bytes_received, sizeof(header) - bytes_received);
        if (valread <= 0) {
            perror("Error reading message header");
            close(client_socket);
            return;
        }
        bytes_received += valread;
    }

    // Validate the message header
    if (header.type != MSG_TYPE_FILE_DATA && header.type != MSG_TYPE_OUTPUT_BINARY) {
        fprintf(stderr, "Invalid message type: %d\n", header.type);
        close(client_socket);
        return;
    }

    if (header.length > MAX_BUFFER_SIZE) {
        fprintf(stderr, "Message length too large: %u\n", header.length);
        close(client_socket);
        return;
    }

    if (header.type == MSG_TYPE_OUTPUT_BINARY) {
        size_t output_binary_length = header.length;
        char *output_binary = (char *)malloc(output_binary_length + 1);

        bytes_received = 0;
        while (bytes_received < output_binary_length) {
            ssize_t valread = read(client_socket, buffer, MAX_BUFFER_SIZE);
            if (valread <= 0) {
                if (valread == 0) {
                    // Client closed the connection unexpectedly
                    fprintf(stderr, "Client closed the connection unexpectedly.\n");
                } else {
                    perror("Error reading output binary name");
                }
                free(output_binary);
                close(client_socket);
                return;
            }
            size_t remaining_bytes = output_binary_length - bytes_received;
            size_t bytes_to_copy = (size_t)valread < remaining_bytes ? (size_t)valread : remaining_bytes;
            memcpy(output_binary + bytes_received, buffer, bytes_to_copy);
            bytes_received += bytes_to_copy;
        }
        output_binary[output_binary_length] = '\0';

        printf("Output binary: %s\n", output_binary);

        char path_to_output_binary[1024]; // Assuming a maximum length of 1024 characters
        snprintf(path_to_output_binary, sizeof(path_to_output_binary), "%s/%s", SERVER_DIRECTORY, output_binary);

        printf("Path to output binary: %s\n", path_to_output_binary);

        char command[2048];
        snprintf(command, 2048, "gcc -c %s %s %s", file_path, "-o", path_to_output_binary);

        printf("Running command: %s\n", command);

        int result = system(command);
        if (result != 0) {
            perror("Compilation failed");
            close(client_socket);
            free(output_binary);
            return;
        }

        // Send compiled file back to client
        FILE *compiled_file = fopen(path_to_output_binary, "rb"); // Open the compiled file
        if (compiled_file == NULL) {
            perror("Error opening compiled file");
            close(client_socket);
            free(output_binary);
            return;
        }

        char send_buffer[MAX_BUFFER_SIZE];
        size_t bytes_read;
        printf("HERERERERE: %lu\n", sizeof(send_buffer));
        while ((bytes_read = fread(send_buffer, 1, sizeof(send_buffer), compiled_file)) > 0) {
            if (send(client_socket, send_buffer, bytes_read, 0) == -1) {
                perror("Error sending compiled file");
                break;
            }
        }
        fclose(compiled_file);

        if (remove(file_path) == 0 && remove(path_to_output_binary) == 0) {
            printf("Files '%s' and '%s' deleted successfully.\n", file_path, path_to_output_binary);
        } else {
            perror("Error deleting file");
        }

        free(output_binary);
    } else {
        perror("Unexpected message type");
        close(client_socket);
        return;
    }

    // Close client socket after sending the file
    close(client_socket);
    free(file_path);
}

int main() {
    createDirectory(SERVER_DIRECTORY);

    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
//    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
//        perror("setsockopt(SO_REUSEADDR) failed");
//        close(server_fd);
//        exit(EXIT_FAILURE);
//    }

    // Initialize address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket to address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        } else {
            printf("ACCEPTED\n");
        }

        // Handle client
        handleClient(client_socket);
    }

    return 0;
}