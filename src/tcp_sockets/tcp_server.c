// TCP Socket Server Example

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    char *hello = "Hello from server";
    
    // Step 1: Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("Socket created successfully\n");
    
    // Optional: Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    // Step 2: Set up server address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Accept connections from any IP
    address.sin_port = htons(PORT);       // Convert to network byte order
    
    // Step 3: Bind socket to the specified IP and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Socket bound to port %d\n", PORT);
    
    // Step 4: Start listening for incoming connections
    // The second parameter is the backlog size - max pending connections queue size
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Server is listening...\n");
    
    // Step 5: Accept an incoming connection
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, 
                            (socklen_t*)&addrlen)) < 0) {
        perror("Accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Connection accepted\n");
    
    // Step 6: Read data from client
    int valread = read(new_socket, buffer, BUFFER_SIZE);
    printf("Message from client: %s\n", buffer);
    
    // Step 7: Send response back to client
    send(new_socket, hello, strlen(hello), 0);
    printf("Hello message sent to client\n");
    
    // Step 8: Clean up
    close(new_socket);
    close(server_fd);
    
    return 0;
}

