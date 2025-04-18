/**
 * @file test_multiplexing.c
 * @brief Unit tests for socket multiplexing functionality
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
#include <sys/time.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include "socket_utils.h"
#include "error_handling.h"
#include "config.h"

#define TEST_PORT 9099
#define MAX_CLIENTS 3
#define TEST_MESSAGE "MULTIPLEX TEST MESSAGE"
#define RESPONSE_PREFIX "RESPONSE TO CLIENT"

/**
 * Structure for client thread arguments
 */
typedef struct {
    int id;
    int port;
} client_args_t;

/**
 * Function to handle test failures
 */
void test_failed(const char *message) {
    fprintf(stderr, "\033[31mTEST FAILED: %s\033[0m\n", message);
    exit(EXIT_FAILURE);
}

/**
 * Run a simple client for multiplexing tests
 */
void *client_thread_func(void *arg) {
    client_args_t *args = (client_args_t *)arg;
    int client_id = args->id;
    int port = args->port;
    
    // Small delay to ensure server is ready
    usleep(100000 * client_id);
    
    // Create socket
    int sockfd = create_tcp_socket(0, 0);
    if (sockfd < 0) {
        perror("Failed to create client socket");
        return NULL;
    }
    
    // Prepare server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    
    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to connect to server");
        close(sockfd);
        return NULL;
    }
    
    // Create unique message for this client
    char message[256];
    sprintf(message, "%s %d", TEST_MESSAGE, client_id);
    
    // Send message to server
    if (send(sockfd, message, strlen(message), 0) <= 0) {
        perror("Failed to send message");
        close(sockfd);
        return NULL;
    }
    
    // Receive response
    char buffer[256] = {0};
    if (recv(sockfd, buffer, sizeof(buffer) - 1, 0) <= 0) {
        perror("Failed to receive response");
        close(sockfd);
        return NULL;
    }
    
    // Verify response
    char expected_response[256];
    sprintf(expected_response, "%s %d", RESPONSE_PREFIX, client_id);
    if (strcmp(buffer, expected_response) != 0) {
        fprintf(stderr, "Client %d: Unexpected response: %s\n", client_id, buffer);
        close(sockfd);
        return NULL;
    }
    
    // Close socket
    close(sockfd);
    return NULL;
}

/**
 * Test basic select() functionality
 */
void test_select_basic() {
    printf("Testing basic select() functionality... ");
    
    // Create two sockets
    int sockfd1 = create_tcp_socket(1, 0);
    int sockfd2 = create_tcp_socket(1, 0);
    
    if (sockfd1 < 0 || sockfd2 < 0) {
        test_failed("Failed to create sockets");
    }
    
    // Prepare fd_set
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd1, &readfds);
    FD_SET(sockfd2, &readfds);
    
    // Set up timeout (0.1 seconds)
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    
    // Call select() - should time out since no activity
    int ret = select(FD_SETSIZE, &readfds, NULL, NULL, &tv);
    
    if (ret != 0) {
        test_failed("select() should have timed out");
    }
    
    // Clean up
    close(sockfd1);
    close(sockfd2);
    
    printf("PASSED\n");
}

/**
 * Test select() with multiple clients
 */
void test_select_multiple_clients() {
    printf("Testing select() with multiple clients... ");
    
    // Create server socket
    int server_fd = create_tcp_socket(1, 0);
    if (server_fd < 0) {
        test_failed("Failed to create server socket");
    }
    
    // Bind server socket
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(TEST_PORT);
    
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(server_fd);
        test_failed("Failed to bind server socket");
    }
    
    // Listen for connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        close(server_fd);
        test_failed("Failed to listen on server socket");
    }
    
    // Create client threads
    pthread_t client_threads[MAX_CLIENTS];
    client_args_t client_args[MAX_CLIENTS];
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_args[i].id = i;
        client_args[i].port = TEST_PORT;
        
        if (pthread_create(&client_threads[i], NULL, client_thread_func, &client_args[i]) != 0) {
            close(server_fd);
            test_failed("Failed to create client thread");
        }
    }
    
    // Set up variables for select()
    fd_set readfds;
    int max_fd = server_fd;
    int client_fds[MAX_CLIENTS] = {-1, -1, -1};
    int clients_handled = 0;
    
    // Main server loop using select()
    while (clients_handled < MAX_CLIENTS) {
        // Set up fd_set for select()
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        
        // Add active client connections to the set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_fds[i] >= 0) {
                FD_SET(client_fds[i], &readfds);
                if (client_fds[i] > max_fd) {
                    max_fd = client_fds[i];
                }
            }
        }
        
        // Wait for activity on any socket (5 second timeout)
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        
        int activity = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        
        if (activity < 0) {
            close(server_fd);
            test_failed("select() failed");
        }
        
        if (activity == 0) {
            // Timeout - something's wrong
            close(server_fd);
            test_failed("select() timed out");
        }
        
        // Check for new connections
        if (FD_ISSET(server_fd, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            
            int new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
            if (new_socket < 0) {
                close(server_fd);
                test_failed("Failed to accept new connection");
            }
            
            // Add new socket to array of client sockets
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_fds[i] < 0) {
                    client_fds[i] = new_socket;
                    break;
                }
            }
        }
        
        // Check for data on client sockets
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_fds[i] >= 0 && FD_ISSET(client_fds[i], &readfds)) {
                char buffer[256] = {0};
                ssize_t bytes_read = recv(client_fds[i], buffer, sizeof(buffer) - 1, 0);
                
                if (bytes_read <= 0) {
                    // Connection closed or error
                    close(client_fds[i]);
                    client_fds[i] = -1;
                } else {
                    // Parse client ID from message
                    int client_id = -1;
                    sscanf(buffer, "%*s %*s %*s %d", &client_id);
                    
                    if (client_id < 0 || client_id >= MAX_CLIENTS) {
                        close(server_fd);
                        test_failed("Invalid client ID received");
                    }
                    
                    // Send response with client ID
                    char response[256];
                    sprintf(response, "%s %d", RESPONSE_PREFIX, client_id);
                    
                    if (send(client_fds[i], response, strlen(response), 0) <= 0) {
                        close(server_fd);
                        test_failed("Failed to send response");
                    }
                    
                    // Close client socket
                    close(client_fds[i]);
                    client_fds[i] = -1;
                    
                    clients_handled++;
                }
            }
        }
    }
    
    // Wait for client threads to finish
    for (int i = 0; i < MAX_CLIENTS; i++) {
        pthread_join(client_threads[i], NULL);
    }
    
    // Close server socket
    close(server_fd);
    
    printf("PASSED\n");
}

/**
 * Test non-blocking I/O with select()
 */
void test_nonblocking_select() {
    printf("Testing non-blocking I/O with select()... ");
    
    // Create socket
    int sockfd = create_tcp_socket(1, 1);  // With non-blocking mode
    if (sockfd < 0) {
        test_failed("Failed to create socket");
    }
    
    // Verify socket is non-blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (!(flags & O_NONBLOCK)) {
        close(sockfd);
        test_failed("Socket is not in non-blocking mode");
    }
    
    // Prepare address structure
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(TEST_PORT + 1);  // Use different port
    
    // Bind socket
    if (bind(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        close(sockfd);
        test_failed("Failed to bind socket");
    }
    
    // Listen for connections
    if (listen(sockfd, 1) < 0) {
        close(sockfd);
        test_failed("Failed to listen on socket");
    }
    
    // Try to accept a connection in non-blocking mode
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_len);
    
    // Should fail with EAGAIN or EWOULDBLOCK since no connections pending
    if (client_fd >= 0) {
        close(client_fd);
        close(sockfd);
        test_failed("Non-blocking accept() should have failed");
    }
    
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
        close(sockfd);
        test_failed("Non-blocking accept() failed with unexpected error code");
    }
    
    // Set up select() to wait for connections
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    
    // Set timeout to 0.1 seconds
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    
    // Call select()
    int ret = select(sockfd + 1, &readfds, NULL, NULL, &tv);
    
    // Should time out with no activity
    if (ret != 0) {
        close(sockfd);
        test_failed("select() should have timed out");
    }
    
    // Clean up
    close(sockfd);
    
    printf("PASSED\n");
}

#ifdef ENABLE_EPOLL
// Only compile this test on Linux where epoll is available
#include <sys/epoll.h>

/**
 * Test basic epoll functionality
 */
void test_epoll_basic() {
    printf("Testing basic epoll functionality... ");
    
    // Create socket
    int sockfd = create_tcp_socket(1, 0);
    if (sockfd < 0) {
        test_failed("Failed to create socket");
    }
    
    // Create epoll instance
    int epollfd = epoll_create1(0);
    if (epollfd < 0) {
        close(sockfd);
        test_failed("Failed to create epoll instance");
    }
    
    // Add socket to epoll instance
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) < 0) {
        close(sockfd);
        close(epollfd);
        test_failed("Failed to add socket to epoll instance");
    }
    
    // Wait for events (should time out)
    struct epoll_event events[1];
    int nfds = epoll_wait(epollfd, events, 1, 100);  // 100ms timeout
    
    if (nfds != 0) {
        close(sockfd);
        close(epollfd);
        test_failed("epoll_wait() should have timed out");
    }
    
    // Remove socket from epoll instance
    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, NULL) < 0) {
        close(sockfd);
        close(epollfd);
        test_failed("Failed to remove socket from epoll instance");
    }
    
    // Clean up
    close(sockfd);
    close(epollfd);
    
    printf("PASSED\n");
}
#endif

int main() {
    printf("Running socket multiplexing tests...\n");
    
    test_select_basic();
    test_nonblocking_select();
    test_select_multiple_clients();
    
#ifdef ENABLE_EPOLL
    test_epoll_basic();
#endif
    
    printf("All socket multiplexing tests PASSED\n");
    return EXIT_SUCCESS;
}