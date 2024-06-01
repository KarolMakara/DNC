//
// Created by kmakara on 01/06/24.
//
#include <stdio.h>
#include <string.h>

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