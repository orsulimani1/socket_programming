# CMake configuration for tests
cmake_minimum_required(VERSION 3.10)

# Add test executables
add_executable(test_tcp test_tcp.c)
add_executable(test_udp test_udp.c)
add_executable(test_multiplexing test_multiplexing.c)

# Link libraries
target_link_libraries(test_tcp socket_common)
target_link_libraries(test_udp socket_common)
target_link_libraries(test_multiplexing socket_common ${CMAKE_THREAD_LIBS_INIT})

# Add tests
add_test(NAME TcpSocketTest COMMAND test_tcp)
add_test(NAME UdpSocketTest COMMAND test_udp)
add_test(NAME MultiplexingTest COMMAND test_multiplexing)

# Test configuration
set_tests_properties(TcpSocketTest PROPERTIES TIMEOUT 5)
set_tests_properties(UdpSocketTest PROPERTIES TIMEOUT 5)
set_tests_properties(MultiplexingTest PROPERTIES TIMEOUT 10)