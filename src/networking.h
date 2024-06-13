//
// Created by kmakara on 04/06/24.
//

#ifndef COMPILATIONSERVER_NETWORKING_H
#define COMPILATIONSERVER_NETWORKING_H

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

int connect_with_timeout(int socket, struct sockaddr_in server_addr, struct timeval timeout);
int remove_socket_timout(int socket);
int receiveACK(int socket);
int isServerReady(int client_socket);
int sendFileToServer(int client_socket, const char *data, size_t data_length, const char *output_binary, uint16_t file_type);
void receiveCompiledFile(int client_socket, const char *output_filename);

#endif //COMPILATIONSERVER_NETWORKING_H
