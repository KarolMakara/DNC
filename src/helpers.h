//
// Created by kmakara on 01/06/24.
//

#ifndef COMPILATIONSERVER_HELPERS_H
#define COMPILATIONSERVER_HELPERS_H

#endif //COMPILATIONSERVER_HELPERS_H

void create_directory(const char *output_file_path, int server);
char* extract_binary_name(const char* path);
int execute_command(const char* command);
void sha256(const char *str, char *hash);
