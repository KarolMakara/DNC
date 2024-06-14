//
// Created by kmakara on 14/06/24.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <sys/time.h>


size_t get_first_digit_milliseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    size_t milliseconds = (tv.tv_usec / 1000) % 10;

    return milliseconds;
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