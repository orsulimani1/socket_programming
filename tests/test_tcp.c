/**
 * @file test_tcp.c
 * @brief Unit tests for TCP socket functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include "socket_utils.h"
#include "error_handling.h"
#include "config.h"

#define TEST_PORT 8899
#define TEST_MESSAGE "TCP TEST MESSAGE"
#define RESPONSE_MESSAGE "TCP TEST RESPONSE"

/**
 * Function to handle test failures
 */
void test_failed(const char *message) {
    fprintf(stderr, "\033[31mTEST FAILED: %s\033[0m\n", message);
    exit(EXIT_FAILURE);
}

/**
 * Run a simple TCP server for testing
 */
void run_test_server(int port) {
    // Create socket
    int server_fd = create_tcp_socket(1, 0);
    if (server_fd < 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // Prepare address structure
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind socket to address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Failed to bind socket");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 1) < 0) {
        perror("Failed to listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Accept a connection
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("Failed to accept connection");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Buffer for receiving data
    char buffer[256] = {0};
    
    // Receive data from client
    ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        perror("Failed to receive data");
        close(client_fd);
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // Verify received message
    if (strcmp(buffer, TEST_MESSAGE) != 0) {
        fprintf(stderr, "Unexpected message: %s\n", buffer);
        close(client_fd);
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // Send response
    if (send(client_fd, RESPONSE_MESSAGE, strlen(RESPONSE_MESSAGE), 0) <= 0) {
        perror("Failed to send response");
        close(client_fd);
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // Close sockets
    close(client_fd);
    close(server_fd);
    
    exit(EXIT_SUCCESS);
}

/**
 * Test TCP socket creation
 */
void test_tcp_socket_creation() {
    printf("Testing TCP socket creation... ");
    
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        test_failed("Failed to create TCP socket");
    }
    
    close(sockfd);
    printf("PASSED\n");
}

/**
 * Test TCP socket connection
 */
void test_tcp_connection() {
    printf("Testing TCP connection... ");
    
    // Fork a child process to run the test server
    pid_t pid = fork();
    if (pid < 0) {
        test_failed("Failed to fork process");
    }
    
    if (pid == 0) {
        // Child process: run server
        run_test_server(TEST_PORT);
        exit(EXIT_SUCCESS);
    } else {
        // Parent process: run client test
        
        // Give the server time to start
        sleep(1);
        
        // Create socket
        int client_fd = create_tcp_socket(0, 0);
        if (client_fd < 0) {
            test_failed("Failed to create client socket");
        }
        
        // Prepare server address structure
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(TEST_PORT);
        inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
        
        // Connect to server
        if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            test_failed("Failed to connect to server");
        }
        
        // Send test message
        if (send(client_fd, TEST_MESSAGE, strlen(TEST_MESSAGE), 0) <= 0) {
            test_failed("Failed to send test message");
        }
        
        // Receive response
        char buffer[256] = {0};
        ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            test_failed("Failed to receive response");
        }
        
        // Verify response
        if (strcmp(buffer, RESPONSE_MESSAGE) != 0) {
            test_failed("Unexpected response message");
        }
        
        // Close socket
        close(client_fd);
        
        // Wait for child process to finish
        int status;
        waitpid(pid, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
            test_failed("Server process failed");
        }
        
        printf("PASSED\n");
    }
}

/**
 * Test TCP socket options
 */
void test_socket_options() {
    printf("Testing TCP socket options... ");
    
    int sockfd = create_tcp_socket(1, 0);
    if (sockfd < 0) {
        test_failed("Failed to create socket");
    }
    
    // Test setting receive timeout
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        test_failed("Failed to set SO_RCVTIMEO option");
    }
    
    // Verify the option was set correctly
    struct timeval tv_get;
    socklen_t len = sizeof(tv_get);
    if (getsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv_get, &len) < 0) {
        test_failed("Failed to get SO_RCVTIMEO option");
    }
    
    if (tv_get.tv_sec != tv.tv_sec) {
        test_failed("SO_RCVTIMEO option not set correctly");
    }
    
    // Test disabling Nagle's algorithm
    int flag = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
        test_failed("Failed to set TCP_NODELAY option");
    }
    
    // Verify the option was set correctly
    int flag_get = 0;
    len = sizeof(flag_get);
    if (getsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag_get, &len) < 0) {
        test_failed("Failed to get TCP_NODELAY option");
    }
    
    if (flag_get != flag) {
        test_failed("TCP_NODELAY option not set correctly");
    }
    
    close(sockfd);
    printf("PASSED\n");
}

int main() {
    printf("Running TCP socket tests...\n");
    
    test_tcp_socket_creation();
    test_socket_options();
    test_tcp_connection();
    
    printf("All TCP socket tests PASSED\n");
    return EXIT_SUCCESS;
}