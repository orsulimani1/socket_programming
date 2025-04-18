/**
 * @file test_udp.c
 * @brief Unit tests for UDP socket functionality
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

#define TEST_PORT 8999
#define TEST_MESSAGE "UDP TEST MESSAGE"
#define RESPONSE_MESSAGE "UDP TEST RESPONSE"

/**
 * Function to handle test failures
 */
void test_failed(const char *message) {
    fprintf(stderr, "\033[31mTEST FAILED: %s\033[0m\n", message);
    exit(EXIT_FAILURE);
}

/**
 * Run a simple UDP server for testing
 */
void run_test_server(int port) {
    // Create socket
    int server_fd = create_udp_socket(0, 0);
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

    // Buffer for receiving data
    char buffer[256] = {0};
    
    // Receive from client
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    ssize_t bytes_received = recvfrom(server_fd, buffer, sizeof(buffer) - 1, 0,
                                    (struct sockaddr *)&client_addr, &client_len);
    if (bytes_received <= 0) {
        perror("Failed to receive data");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // Verify received message
    if (strcmp(buffer, TEST_MESSAGE) != 0) {
        fprintf(stderr, "Unexpected message: %s\n", buffer);
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // Send response
    if (sendto(server_fd, RESPONSE_MESSAGE, strlen(RESPONSE_MESSAGE), 0,
            (struct sockaddr *)&client_addr, client_len) <= 0) {
        perror("Failed to send response");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // Close socket
    close(server_fd);
    
    exit(EXIT_SUCCESS);
}

/**
 * Test UDP socket creation
 */
void test_udp_socket_creation() {
    printf("Testing UDP socket creation... ");
    
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        test_failed("Failed to create UDP socket");
    }
    
    close(sockfd);
    printf("PASSED\n");
}

/**
 * Test UDP socket communication
 */
void test_udp_communication() {
    printf("Testing UDP communication... ");
    
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
        int client_fd = create_udp_socket(0, 0);
        if (client_fd < 0) {
            test_failed("Failed to create client socket");
        }
        
        // Prepare server address structure
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(TEST_PORT);
        inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
        
        // Send test message
        if (sendto(client_fd, TEST_MESSAGE, strlen(TEST_MESSAGE), 0,
                (struct sockaddr *)&server_addr, sizeof(server_addr)) <= 0) {
            test_failed("Failed to send test message");
        }
        
        // Receive response
        char buffer[256] = {0};
        struct sockaddr_in response_addr;
        socklen_t addr_len = sizeof(response_addr);
        
        // Set receive timeout
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        
        ssize_t bytes_received = recvfrom(client_fd, buffer, sizeof(buffer) - 1, 0,
                                        (struct sockaddr *)&response_addr, &addr_len);
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
 * Test UDP socket options
 */
void test_udp_socket_options() {
    printf("Testing UDP socket options... ");
    
    int sockfd = create_udp_socket(0, 0);
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
    
    // Test setting broadcast option
    int broadcast = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        test_failed("Failed to set SO_BROADCAST option");
    }
    
    // Verify the option was set correctly
    int broadcast_get = 0;
    len = sizeof(broadcast_get);
    if (getsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast_get, &len) < 0) {
        test_failed("Failed to get SO_BROADCAST option");
    }
    
    if (broadcast_get != broadcast) {
        test_failed("SO_BROADCAST option not set correctly");
    }
    
    close(sockfd);
    printf("PASSED\n");
}

/**
 * Test UDP broadcast functionality
 */
void test_udp_broadcast() {
    printf("Testing UDP broadcast functionality... ");
    
    // This test is simplified since we can't guarantee a broadcast receiver
    // We just verify that setting up a broadcast socket works
    
    int sockfd = create_udp_socket(1, 0);  // Create with broadcast enabled
    if (sockfd < 0) {
        test_failed("Failed to create broadcast socket");
    }
    
    // Verify that broadcast option is set
    int broadcast = 0;
    socklen_t len = sizeof(broadcast);
    if (getsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, &len) < 0) {
        test_failed("Failed to get SO_BROADCAST option");
    }
    
    if (!broadcast) {
        test_failed("SO_BROADCAST option not set correctly");
    }
    
    close(sockfd);
    printf("PASSED\n");
}

int main() {
    printf("Running UDP socket tests...\n");
    
    test_udp_socket_creation();
    test_udp_socket_options();
    test_udp_communication();
    test_udp_broadcast();
    
    printf("All UDP socket tests PASSED\n");
    return EXIT_SUCCESS;
}