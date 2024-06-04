# Specify the source files
set(SERVER_SOURCE dncs.c
        message.h
        message.c
        constants.h
        file_operations.c
        file_operations.h
        networking.h
        networking.c)
set(CLIENT_SOURCE dncc.c
        message.h
        message.c
        constants.h
        file_operations.c
        file_operations.h
        networking.c
        networking.h)

# Create executable for the server
add_executable(dncs ${SERVER_SOURCE})
target_link_libraries(dncs OpenSSL::SSL OpenSSL::Crypto)

# Create executable for the client
add_executable(dncc ${CLIENT_SOURCE})
