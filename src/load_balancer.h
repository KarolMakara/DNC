//
// Created by kmakara on 14/06/24.
//

#ifndef COMPILATIONSERVER_LOAD_BALANCER_H
#define COMPILATIONSERVER_LOAD_BALANCER_H

#endif //COMPILATIONSERVER_LOAD_BALANCER_H

size_t get_first_digit_milliseconds();
void free_arr(char **server_list, int server_count);
char **read_server_list(const char *filename, int *server_count);