//
// Created by kmakara on 01/06/24.
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include "helpers.h"
#include "constants.h"


void create_directory(const char *output_file_path, int server) {
    if (server == 0) {
        if (mkdir(output_file_path, 0777) == -1 && errno != EEXIST) {
            perror("Error creating directory");
            return;
        }
    }
    char *buffer = strdup(output_file_path);
    if (buffer == NULL) {
        perror("Error duplicating string");
        return;
    }

    char *last_slash = strrchr(buffer, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';

        struct stat st;
        if (stat(buffer, &st) != 0) {
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


char* extract_binary_name(const char* path) {
    static char binary_name[MAX_BINARY_NAME_LENGTH];
    const char* last_separator = strrchr(path, '/');
    if (last_separator == NULL) {
        strncpy(binary_name, path, MAX_BINARY_NAME_LENGTH - 1);
        binary_name[MAX_BINARY_NAME_LENGTH - 1] = '\0';
    } else {
        strncpy(binary_name, last_separator + 1, MAX_BINARY_NAME_LENGTH - 1);
        binary_name[MAX_BINARY_NAME_LENGTH - 1] = '\0';
    }
    return binary_name;
}



int execute_command(const char* command) {
    pid_t pid;
    int status;
    char* argv[] = {"/bin/sh", "-c", (char*)command, NULL};

    pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    } else if (pid == 0) {
        execvp(argv[0], argv);
        perror("execvp");
        exit(1);
    } else {
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            int exitStatus = WEXITSTATUS(status);
            if (exitStatus != 0) {
                fprintf(stderr, "Command failed with exit status %d\n", exitStatus);
                return -1;
            }
        } else {
            fprintf(stderr, "Command terminated abnormally\n");
            return -1;
        }
    }
    return 0;
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