// Socket Multiplexing with epoll() Example (Linux-specific)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#define PORT 8080
#define MAX_EVENTS 10
#define BUFFER_SIZE 1024

// Set socket to non-blocking mode
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL O_NONBLOCK");
        return -1;
    }
    return 0;
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    
    char buffer[BUFFER_SIZE];
    char *welcome_message = "Welcome to the epoll server\n";
    
    // Step 1: Create TCP socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("Socket created successfully\n");
    
    // Step 2: Set socket options to allow reuse of address and port
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    // Step 3: Set up server address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    // Step 4: Bind socket to the specified IP and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Socket bound to port %d\n", PORT);
    
    // Step 5: Start listening for incoming connections
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    printf("Server is listening...\n");
    
    // Step 6: Make the server socket non-blocking
    if (set_nonblocking(server_fd) < 0) {
        perror("Failed to set server socket to non-blocking");
        exit(EXIT_FAILURE);
    }
    
    // Step 7: Create epoll instance
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");
        exit(EXIT_FAILURE);
    }
    
    // Step 8: Add server socket to epoll instance
    struct epoll_event ev;
    ev.events = EPOLLIN;  // Available for read operations
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        perror("epoll_ctl: server_fd");
        exit(EXIT_FAILURE);
    }
    
    // Buffer to store events
    struct epoll_event events[MAX_EVENTS];
    
    printf("Waiting for connections...\n");
    
    while (1) {
        // Step 9: Wait for events on any of the registered file descriptors
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events == -1) {
            perror("epoll_wait failed");
            exit(EXIT_FAILURE);
        }
        
        // Step 10: Process events
        for (int i = 0; i < num_events; i++) {
            // If event on server socket -> new connection
            if (events[i].data.fd == server_fd) {
                // Accept new connections
                while (1) {
                    int client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
                    if (client_fd == -1) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            // No more connections to accept
                            break;
                        } else {
                            perror("accept failed");
                            break;
                        }
                    }
                    
                    // Print connection info
                    printf("New connection from %s:%d\n", 
                           inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                    
                    // Make client socket non-blocking
                    if (set_nonblocking(client_fd) < 0) {
                        perror("Failed to set client socket to non-blocking");
                        close(client_fd);
                        continue;
                    }
                    
                    // Send welcome message
                    if (send(client_fd, welcome_message, strlen(welcome_message), 0) != strlen(welcome_message)) {
                        perror("Send welcome message failed");
                    }
                    
                    // Add client socket to epoll
                    ev.events = EPOLLIN | EPOLLET;  // Edge-triggered mode
                    ev.data.fd = client_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                        perror("epoll_ctl: client_fd");
                        close(client_fd);
                        continue;
                    }
                }
            } else {
                // Data from an existing client
                int client_fd = events[i].data.fd;
                
                // Read data in a loop (for edge-triggered mode, must read all data)
                while (1) {
                    memset(buffer, 0, BUFFER_SIZE);
                    ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
                    
                    if (bytes_read == -1) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            // No more data to read
                            break;
                        } else {
                            perror("read failed");
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                            close(client_fd);
                            break;
                        }
                    } else if (bytes_read == 0) {
                        // Client closed connection
                        printf("Client disconnected\n");
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                        close(client_fd);
                        break;
                    } else {
                        // Process data
                        buffer[bytes_read] = '\0';
                        printf("Received from client %d: %s\n", client_fd, buffer);
                        
                        // Echo back
                        send(client_fd, buffer, bytes_read, 0);
                    }
                }
            }
        }
    }
    
    // Cleanup
    close(server_fd);
    close(epoll_fd);
    
    return 0;
}