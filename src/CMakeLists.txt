# Specify the source files
set(SERVER_SOURCE dncs.c
        message.h
        message.c
        constants.h
        file_operations.c
        file_operations.h)
set(CLIENT_SOURCE dncc.c
        message.h
        message.c
        constants.h
        file_operations.c
        file_operations.h
        connect_wait.c
        connect_wait.h)

# Create executable for the server
add_executable(dncs ${SERVER_SOURCE})
target_link_libraries(dncs OpenSSL::SSL OpenSSL::Crypto)

# Create executable for the client
add_executable(dncc ${CLIENT_SOURCE})
