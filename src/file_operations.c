//
// Created by kmakara on 01/06/24.
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

#include "file_operations.h"

int check_file_type(const char *file_path) {
    FILE *file;
    char line[256];

    file = fopen(file_path, "r");
    if (file == NULL) {
        perror("Error opening file");
        return -1;
    }
    if (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, ".cpp") != NULL) {
            fclose(file);
            return FILE_TYPE_CPP;
        } else if (strstr(line, ".c") != NULL) {
            fclose(file);
            return FILE_TYPE_C;
        }

    } else {
        fclose(file);
        return -1;
    }

    fclose(file);
    return -1;
}

void create_directory(const char *output_file_path) {
    char *buffer = strdup(output_file_path);
    if (buffer == NULL) {
        perror("Error duplicating string");
        return;
    }

    char *last_slash = strrchr(buffer, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';

        // Check if directory exists
        struct stat st;
        if (stat(buffer, &st) != 0) {
            // Directory does not exist, attempt to create it
            if (mkdir(buffer, 0777) == -1 && errno != EEXIST) {
                perror("Error creating directory");
                free(buffer);
                return;
            }
        } else if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "Error: %s exists but is not a directory\n", buffer);
            free(buffer);
            return;
        }

        *last_slash = '/';
    }

    free(buffer);
}