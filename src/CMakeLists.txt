set(SERVER_SOURCE dncs.c
        message.h
        message.c
        constants.h
        helpers.c
        helpers.h
        networking.h
        networking.c)
set(CLIENT_SOURCE dncc.c
        message.h
        message.c
        constants.h
        helpers.c
        helpers.h
        networking.c
        networking.h
        load_balancer.c
        load_balancer.h)

add_executable(dncs ${SERVER_SOURCE})
target_link_libraries(dncs OpenSSL::SSL OpenSSL::Crypto)

add_executable(dncc ${CLIENT_SOURCE})
target_link_libraries(dncc OpenSSL::SSL OpenSSL::Crypto)
