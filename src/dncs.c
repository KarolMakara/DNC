#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include "message.h"
#include "constants.h"
#include "helpers.h"
#include "networking.h"


size_t client_count = 0;


void sigchld_handler(__attribute__((unused)) int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        client_count--;
    }
}


int main() {
    create_directory(SERVER_DIRECTORY, 0);

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
            handle_client(client_socket);
            client_count--;
            exit(EXIT_SUCCESS);
        } else {
            close(client_socket);
        }
    }
}