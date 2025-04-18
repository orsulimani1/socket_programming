# CMake configuration for examples subdirectory
cmake_minimum_required(VERSION 3.10)

# Find OpenSSL for secure socket examples
find_package(OpenSSL)

# Add examples that work on all platforms
add_executable(socket_client_server advanced/socket_client_server.c)
add_executable(tcp_nonblocking advanced/tcp_nonblocking.c)
add_executable(udp_broadcast advanced/udp_broadcast.c)

# Link the common library to all examples
target_link_libraries(socket_client_server socket_common)
target_link_libraries(tcp_nonblocking socket_common)
target_link_libraries(udp_broadcast socket_common)

# Add the pthread library to examples that need it
target_link_libraries(tcp_nonblocking ${CMAKE_THREAD_LIBS_INIT})

# Conditionally build examples that require OpenSSL
if(OPENSSL_FOUND)
    message(STATUS "OpenSSL found. Building TLS socket examples.")
    
    add_executable(tls_server advanced/tls_server.c)
    add_executable(tls_client advanced/tls_client.c)
    add_executable(socket_encryption advanced/socket_encryption.c)
    
    # Include OpenSSL headers
    target_include_directories(tls_server PRIVATE ${OPENSSL_INCLUDE_DIR})
    target_include_directories(tls_client PRIVATE ${OPENSSL_INCLUDE_DIR})
    target_include_directories(socket_encryption PRIVATE ${OPENSSL_INCLUDE_DIR})
    
    # Link OpenSSL libraries
    target_link_libraries(tls_server ${OPENSSL_LIBRARIES} socket_common)
    target_link_libraries(tls_client ${OPENSSL_LIBRARIES} socket_common)
    target_link_libraries(socket_encryption ${OPENSSL_LIBRARIES} socket_common)
    
    # Install secure socket examples
    install(TARGETS 
        tls_server tls_client socket_encryption
        DESTINATION bin/advanced)
endif()

# Conditionally build Linux-specific examples
if(UNIX AND NOT APPLE)
    message(STATUS "Building Linux-specific socket examples")
    
    # Add Linux-specific examples
    add_executable(sctp_server advanced/sctp_server.c)
    add_executable(sctp_client advanced/sctp_client.c)
    add_executable(raw_socket advanced/raw_socket.c)
    add_executable(unix_socket_credentials advanced/unix_socket_credentials.c)
    
    # Link libraries
    target_link_libraries(sctp_server socket_common sctp)
    target_link_libraries(sctp_client socket_common sctp)
    target_link_libraries(raw_socket socket_common)
    target_link_libraries(unix_socket_credentials socket_common)
    
    # Raw socket example needs root privileges
    add_custom_command(TARGET raw_socket POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Note: raw_socket example needs to be run with sudo")
        
    # Install Linux-specific examples
    install(TARGETS 
        sctp_server sctp_client raw_socket unix_socket_credentials
        DESTINATION bin/advanced)
endif()

# Install platform-independent examples
install(TARGETS 
    socket_client_server tcp_nonblocking udp_broadcast
    DESTINATION bin/advanced)