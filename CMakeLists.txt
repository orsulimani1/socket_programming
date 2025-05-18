cmake_minimum_required(VERSION 3.10)
project(Socket_Programming_Examples C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")
endif()

# Find required packages
find_package(Threads REQUIRED)

# Add include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Define source directories
set(TCP_SRC src/tcp_sockets)
set(UDP_SRC src/udp_sockets)
set(UDS_SRC src/unix_domain_sockets)
set(MULTIPLEX_SRC src/multiplexing)
set(ZEROCOPY_SRC src/zerocopy)

# TCP socket examples
add_executable(tcp_server ${TCP_SRC}/tcp_server.c)
add_executable(tcp_client ${TCP_SRC}/tcp_client.c)

# UDP socket examples
add_executable(udp_server ${UDP_SRC}/udp_server.c)
add_executable(udp_client ${UDP_SRC}/udp_client.c)

# Unix Domain Socket examples
add_executable(uds_server ${UDS_SRC}/uds_server.c)
add_executable(uds_client ${UDS_SRC}/uds_client.c)

# Multiplexing examples
add_executable(select_server ${MULTIPLEX_SRC}/select_server.c)
add_executable(epoll_server ${MULTIPLEX_SRC}/epoll_server.c)

# Zero-copy examples
add_executable(zero_copy_sendfile ${ZEROCOPY_SRC}/zero_copy_sendfile.c)
add_executable(zero_copy_mmap ${ZEROCOPY_SRC}/zero_copy_mmap.c)
add_executable(zero_copy_client ${ZEROCOPY_SRC}/zero_copy_client.c)

# Real-world examples
add_executable(sensor_monitoring src/examples/sensor_monitoring.c)
add_executable(high_perf_webserver src/examples/high_perf_webserver.c)
add_executable(can_automotive src/examples/can_automotive.c)
add_executable(low_latency_trading src/examples/low_latency_trading.c)


add_subdirectory(examples)
# Link libraries
if(UNIX AND NOT APPLE)
    # Linux-specific libraries
    target_link_libraries(epoll_server ${CMAKE_THREAD_LIBS_INIT})
    target_link_libraries(zero_copy_sendfile ${CMAKE_THREAD_LIBS_INIT})
    target_link_libraries(high_perf_webserver ${CMAKE_THREAD_LIBS_INIT})
    
    # For CAN sockets example (requires socketcan)
    target_link_libraries(can_automotive ${CMAKE_THREAD_LIBS_INIT})
endif()

# Link pthread to multithreaded examples
target_link_libraries(select_server ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(sensor_monitoring ${CMAKE_THREAD_LIBS_INIT})

# Installation rules
install(TARGETS 
    tcp_server tcp_client 
    udp_server udp_client 
    uds_server uds_client 
    select_server
    zero_copy_sendfile zero_copy_client
    sensor_monitoring high_perf_webserver
    DESTINATION bin)

# Conditionally install Linux-specific examples
if(UNIX AND NOT APPLE)
    install(TARGETS 
        epoll_server 
        zero_copy_mmap
        can_automotive
        DESTINATION bin)
endif()

# Create a header-only library for common functions
add_library(socket_common INTERFACE)
target_include_directories(socket_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add subdirectory for additional examples
# add_subdirectory(examples)

# Testing
enable_testing()
add_subdirectory(tests)

# Documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_INPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs)
    set(DOXYGEN_INDEX_FILE ${DOXYGEN_OUTPUT_DIR}/html/index.html)
    set(DOXYGEN_CONFIG_FILE ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${DOXYGEN_CONFIG_FILE} @ONLY)
    
    add_custom_command(OUTPUT ${DOXYGEN_INDEX_FILE}
                      COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_CONFIG_FILE}
                      MAIN_DEPENDENCY ${DOXYGEN_CONFIG_FILE}
                      COMMENT "Generating API documentation with Doxygen")
    
    add_custom_target(docs DEPENDS ${DOXYGEN_INDEX_FILE})
endif()

# Print configuration summary
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_C_COMPILER: ${CMAKE_C_COMPILER}")
message(STATUS "CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")