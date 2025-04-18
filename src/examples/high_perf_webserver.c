/**
 * @file high_perf_webserver.c
 * @brief High-performance web server using epoll
 * 
 * This example demonstrates a high-performance web server capable of handling
 * thousands of concurrent connections using the epoll multiplexing method.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "socket_utils.h"
#include "error_handling.h"
#include "config.h"

#ifdef ENABLE_EPOLL
#include <sys/epoll.h>

#define HTTP_PORT 8080
#define MAX_EVENTS 1024
#define MAX_CONNECTIONS 10000
#define BUFFER_SIZE 8192
#define WEB_ROOT "./www"

// Flag for graceful shutdown
static volatile int keep_running = 1;

// Connection structure
typedef struct {
    int fd;                      // Socket file descriptor
    char buffer[BUFFER_SIZE];    // Buffer for incoming data
    size_t buffer_used;          // Amount of data in the buffer
    size_t response_sent;        // Amount of response data sent
    char *response;              // HTTP response data
    size_t response_size;        // Total size of response
    int keep_alive;              // Keep-alive flag
} connection_t;

// Connection pool
connection_t *connections;

// Signal handler for graceful shutdown
void handle_signal(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    keep_running = 0;
}

// Initialize connections pool
void init_connections() {
    connections = calloc(MAX_CONNECTIONS, sizeof(connection_t));
    if (!connections) {
        FATAL("Failed to allocate connection pool");
    }
    
    // Initialize all connections
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        connections[i].fd = -1;  // Mark as unused
    }
}

// Free connections pool
void free_connections() {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i].fd >= 0) {
            close(connections[i].fd);
        }
        free(connections[i].response);
    }
    free(connections);
}

// Add new connection to pool
connection_t *add_connection(int fd) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i].fd < 0) {
            // Found an unused slot
            connections[i].fd = fd;
            connections[i].buffer_used = 0;
            connections[i].response = NULL;
            connections[i].response_size = 0;
            connections[i].response_sent = 0;
            connections[i].keep_alive = 0;
            return &connections[i];
        }
    }
    
    // No slots available
    return NULL;
}

// Remove connection from pool
void remove_connection(connection_t *conn) {
    if (!conn) return;
    
    if (conn->fd >= 0) {
        close(conn->fd);
    }
    
    free(conn->response);
    conn->fd = -1;
    conn->buffer_used = 0;
    conn->response = NULL;
    conn->response_size = 0;
    conn->response_sent = 0;
    conn->keep_alive = 0;
}

// Parse HTTP request and generate response
void process_http_request(connection_t *conn) {
    // Null-terminate the buffer for string operations
    conn->buffer[conn->buffer_used] = '\0';
    
    // Check if it's a GET request
    if (strncmp(conn->buffer, "GET ", 4) != 0) {
        // Only support GET requests in this example
        const char *response = "HTTP/1.1 501 Not Implemented\r\n"
                            "Content-Length: 28\r\n"
                            "Connection: close\r\n"
                            "\r\n"
                            "Only GET requests supported";
        
        conn->response = strdup(response);
        conn->response_size = strlen(response);
        conn->response_sent = 0;
        conn->keep_alive = 0;
        return;
    }
    
    // Parse the requested path
    char path[BUFFER_SIZE] = {0};
    sscanf(conn->buffer, "GET %s", path);
    
    // Check for "Connection: keep-alive" header
    conn->keep_alive = (strstr(conn->buffer, "Connection: keep-alive") != NULL);
    
    // Default to index.html for root path
    if (strcmp(path, "/") == 0) {
        strcpy(path, "/index.html");
    }
    
    // Construct full file path
    char full_path[BUFFER_SIZE];
    snprintf(full_path, sizeof(full_path), "%s%s", WEB_ROOT, path);
    
    // Try to open the requested file
    int file_fd = open(full_path, O_RDONLY);
    if (file_fd < 0) {
        // File not found
        const char *response = "HTTP/1.1 404 Not Found\r\n"
                            "Content-Length: 14\r\n"
                            "Connection: close\r\n"
                            "\r\n"
                            "File not found";
        
        conn->response = strdup(response);
        conn->response_size = strlen(response);
        conn->response_sent = 0;
        conn->keep_alive = 0;
        return;
    }
    
    // Get file size
    struct stat file_stat;
    fstat(file_fd, &file_stat);
    off_t file_size = file_stat.st_size;
    
    // Determine content type based on file extension
    const char *content_type = "application/octet-stream";  // Default
    if (strstr(path, ".html") || strstr(path, ".htm")) {
        content_type = "text/html";
    } else if (strstr(path, ".css")) {
        content_type = "text/css";
    } else if (strstr(path, ".js")) {
        content_type = "application/javascript";
    } else if (strstr(path, ".jpg") || strstr(path, ".jpeg")) {
        content_type = "image/jpeg";
    } else if (strstr(path, ".png")) {
        content_type = "image/png";
    } else if (strstr(path, ".gif")) {
        content_type = "image/gif";
    } else if (strstr(path, ".txt")) {
        content_type = "text/plain";
    }
    
    // Generate HTTP header
    char header[BUFFER_SIZE];
    int header_len = snprintf(header, sizeof(header),
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: %s\r\n"
                            "Content-Length: %ld\r\n"
                            "Connection: %s\r\n"
                            "\r\n",
                            content_type, (long)file_size,
                            conn->keep_alive ? "keep-alive" : "close");
    
    // Allocate memory for complete response (header + file content)
    conn->response_size = header_len + file_size;
    conn->response = malloc(conn->response_size);
    if (!conn->response) {
        close(file_fd);
        // Memory allocation failed, send error response
        const char *response = "HTTP/1.1 500 Internal Server Error\r\n"
                            "Content-Length: 21\r\n"
                            "Connection: close\r\n"
                            "\r\n"
                            "Internal Server Error";
        
        conn->response = strdup(response);
        conn->response_size = strlen(response);
        conn->response_sent = 0;
        conn->keep_alive = 0;
        return;
    }
    
    // Copy header to response buffer
    memcpy(conn->response, header, header_len);
    
    // Read file content into response buffer
    ssize_t bytes_read = read(file_fd, conn->response + header_len, file_size);
    close(file_fd);
    
    if (bytes_read != file_size) {
        // Error reading file
        free(conn->response);
        const char *response = "HTTP/1.1 500 Internal Server Error\r\n"
                            "Content-Length: 21\r\n"
                            "Connection: close\r\n"
                            "\r\n"
                            "Internal Server Error";
        
        conn->response = strdup(response);
        conn->response_size = strlen(response);
        conn->keep_alive = 0;
    }
    
    conn->response_sent = 0;
}

// Send HTTP response to client
int send_response(connection_t *conn, int epoll_fd) {
    if (!conn || !conn->response) return -1;
    
    // Calculate remaining data to send
    size_t remaining = conn->response_size - conn->response_sent;
    
    // Send data to client
    ssize_t bytes_sent = send(conn->fd, conn->response + conn->response_sent, remaining, 0);
    
    if (bytes_sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Would block, try again later
            return 0;
        }
        // Error sending data
        return -1;
    }
    
    // Update sent counter
    conn->response_sent += bytes_sent;
    
    // Check if all data has been sent
    if (conn->response_sent >= conn->response_size) {
        // Response fully sent
        free(conn->response);
        conn->response = NULL;
        conn->response_size = 0;
        
        if (conn->keep_alive) {
            // Reset for next request
            conn->buffer_used = 0;
            
            // Modify epoll event to wait for more data
            struct epoll_event ev;
            ev.events = EPOLLIN | EPOLLET;
            ev.data.ptr = conn;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev) < 0) {
                LOG_ERRNO("epoll_ctl error");
                return -1;
            }
            
            return 0;
        } else {
            // Close connection
            return -1;
        }
    }
    
    return 0;
}

// Set socket to non-blocking mode
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        LOG_ERRNO("fcntl F_GETFL error");
        return -1;
    }
    
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        LOG_ERRNO("fcntl F_SETFL O_NONBLOCK error");
        return -1;
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    int port = HTTP_PORT;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [-p port] [--help]\n", argv[0]);
            printf("  -p port   : Port to listen on (default: %d)\n", HTTP_PORT);
            printf("  -h, --help: Show this help message\n");
            return 0;
        }
    }
    
    // Set up signal handling for graceful shutdown
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // Initialize connections pool
    init_connections();
    
    // Create server socket
    int server_fd = create_tcp_socket(1, 0);  // With SO_REUSEADDR, blocking mode
    if (server_fd < 0) {
        FATAL_ERRNO("Failed to create socket");
    }
    
    // Set socket to non-blocking mode
    if (set_nonblocking(server_fd) < 0) {
        close(server_fd);
        FATAL("Failed to set non-blocking mode");
    }
    
    // Prepare address structure for binding
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    // Bind socket to port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        close(server_fd);
        FATAL_ERRNO("Failed to bind to port %d", port);
    }
    
    // Start listening for connections
    if (listen(server_fd, SOMAXCONN) < 0) {
        close(server_fd);
        FATAL_ERRNO("Failed to listen on socket");
    }
    
    printf("High-performance web server started on port %d\n", port);
    printf("Server root directory: %s\n", WEB_ROOT);
    printf("Press Ctrl+C to shut down\n");
    
    // Create epoll instance
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        close(server_fd);
        FATAL_ERRNO("Failed to create epoll instance");
    }
    
    // Add server socket to epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;  // Edge-triggered mode
    ev.data.ptr = NULL;  // NULL indicates it's the server socket
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
        close(epoll_fd);
        close(server_fd);
        FATAL_ERRNO("Failed to add server socket to epoll");
    }
    
    // Event loop
    struct epoll_event events[MAX_EVENTS];
    int connection_count = 0;
    
    while (keep_running) {
        // Wait for events
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);  // 1 second timeout
        
        if (nfds < 0 && errno != EINTR) {
            LOG_ERRNO("epoll_wait error");
            break;
        }
        
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.ptr == NULL) {
                // Event on server socket - accept new connections
                while (1) {
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    
                    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                    
                    if (client_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // No more connections to accept
                            break;
                        } else {
                            LOG_ERRNO("accept error");
                            break;
                        }
                    }
                    
                    // Set new socket to non-blocking
                    if (set_nonblocking(client_fd) < 0) {
                        close(client_fd);
                        continue;
                    }
                    
                    // Disable Nagle's algorithm for lower latency
                    int flag = 1;
                    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
                    
                    // Add new connection to pool
                    connection_t *conn = add_connection(client_fd);
                    if (!conn) {
                        // Connection pool full
                        const char *msg = "HTTP/1.1 503 Service Unavailable\r\n"
                                        "Content-Length: 21\r\n"
                                        "Connection: close\r\n"
                                        "\r\n"
                                        "Server is overloaded";
                        send(client_fd, msg, strlen(msg), 0);
                        close(client_fd);
                        continue;
                    }
                    
                    // Add socket to epoll instance
                    struct epoll_event ev;
                    ev.events = EPOLLIN | EPOLLET;  // Edge-triggered mode
                    ev.data.ptr = conn;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
                        LOG_ERRNO("epoll_ctl error");
                        remove_connection(conn);
                        continue;
                    }
                    
                    // Log connection
                    char client_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
                    printf("New connection from %s:%d\n", client_ip, ntohs(client_addr.sin_port));
                    
                    connection_count++;
                }
            } else {
                // Event on client socket
                connection_t *conn = (connection_t *)events[i].data.ptr;
                
                if (events[i].events & EPOLLIN) {
                    // Socket ready for reading
                    if (conn->response) {
                        // Already processing a request, ignore additional data
                        continue;
                    }
                    
                    // Read data from socket
                    ssize_t bytes_read = recv(conn->fd, conn->buffer + conn->buffer_used, 
                                            BUFFER_SIZE - conn->buffer_used - 1, 0);
                    
                    if (bytes_read < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // No more data to read
                            continue;
                        } else {
                            // Error reading from socket
                            remove_connection(conn);
                            connection_count--;
                            continue;
                        }
                    } else if (bytes_read == 0) {
                        // Connection closed by client
                        remove_connection(conn);
                        connection_count--;
                        continue;
                    }
                    
                    // Update buffer usage
                    conn->buffer_used += bytes_read;
                    
                    // Check if we have a complete HTTP request
                    if (strstr(conn->buffer, "\r\n\r\n") != NULL) {
                        // Complete HTTP request received, process it
                        process_http_request(conn);
                        
                        // Change to write mode
                        struct epoll_event ev;
                        ev.events = EPOLLOUT | EPOLLET;  // Edge-triggered mode
                        ev.data.ptr = conn;
                        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev) < 0) {
                            LOG_ERRNO("epoll_ctl error");
                            remove_connection(conn);
                            connection_count--;
                            continue;
                        }
                    } else if (conn->buffer_used >= BUFFER_SIZE - 1) {
                        // Buffer full but no complete request, send error
                        const char *response = "HTTP/1.1 413 Request Entity Too Large\r\n"
                                            "Content-Length: 24\r\n"
                                            "Connection: close\r\n"
                                            "\r\n"
                                            "Request is too large";
                        
                        conn->response = strdup(response);
                        conn->response_size = strlen(response);
                        conn->response_sent = 0;
                        conn->keep_alive = 0;
                        
                        // Change to write mode
                        struct epoll_event ev;
                        ev.events = EPOLLOUT | EPOLLET;  // Edge-triggered mode
                        ev.data.ptr = conn;
                        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev) < 0) {
                            LOG_ERRNO("epoll_ctl error");
                            remove_connection(conn);
                            connection_count--;
                            continue;
                        }
                    }
                } else if (events[i].events & EPOLLOUT) {
                    // Socket ready for writing
                    if (send_response(conn, epoll_fd) < 0) {
                        remove_connection(conn);
                        connection_count--;
                    }
                }
                
                // Check for errors
                if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    remove_connection(conn);
                    connection_count--;
                }
            }
        }
        
        // Log status periodically
        static time_t last_status = 0;
        time_t now = time(NULL);
        if (now - last_status >= 10) {  // Every 10 seconds
            printf("Status: %d active connections\n", connection_count);
            last_status = now;
        }
    }
    
    // Clean up
    printf("Shutting down web server...\n");
    close(epoll_fd);
    close(server_fd);
    free_connections();
    
    printf("Web server stopped\n");
    
    return 0;
}

#else  // !ENABLE_EPOLL

// Simplified version for systems without epoll
int main(int argc, char *argv[]) {
    printf("This high-performance web server requires epoll, which is only available on Linux.\n");
    printf("Please enable EPOLL in config.h and recompile.\n");
    return 1;
}

#endif  // ENABLE_EPOLL