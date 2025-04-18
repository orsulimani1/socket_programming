// Unix Domain Socket Server Example

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/uds_socket"
#define BUFFER_SIZE 1024

int main() {
    int server_fd, client_fd;
    struct sockaddr_un server_addr, client_addr;
    char buffer[BUFFER_SIZE];
    socklen_t client_len;
    
    // Step 1: Create Unix domain socket file descriptor
    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("Unix domain socket created successfully\n");
    
    // Step 2: Set up server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);
    
    // Step 3: Remove any existing socket file
    unlink(SOCKET_PATH);
    
    // Step 4: Bind socket to the file path
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Socket bound to path: %s\n", SOCKET_PATH);
    
    // Step 5: Start listening for incoming connections
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Server is listening...\n");
    
    // Step 6: Accept an incoming connection
    client_len = sizeof(client_addr);
    if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) < 0) {
        perror("Accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Connection accepted\n");
    
    // Step 7: Read data from client
    ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';  // Null terminate
        printf("Message from client: %s\n", buffer);
        
        // Step 8: Send response back to client
        char *response = "Hello from UDS server";
        write(client_fd, response, strlen(response));
        printf("Response sent to client\n");
    }
    
    // Step 9: Clean up
    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);  // Remove socket file
    
    return 0;
}