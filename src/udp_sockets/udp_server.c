// UDP Socket Server Example

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE] = {0};
    char *hello = "Hello from UDP server";
    
    // Step 1: Create UDP socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("UDP socket created successfully\n");
    
    // Step 2: Set up server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));
    
    server_addr.sin_family = AF_INET;  // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Accept connections from any IP
    server_addr.sin_port = htons(PORT);  // Convert to network byte order
    
    // Step 3: Bind socket to the specified IP and port
    if (bind(server_fd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Socket bound to port %d\n", PORT);
    
    // Step 4: Receive message from client
    int len = sizeof(client_addr);
    int n = recvfrom(server_fd, buffer, BUFFER_SIZE, 
                    MSG_WAITALL, (struct sockaddr *)&client_addr, &len);
    buffer[n] = '\0';  // Null terminate the received data
    printf("Message from client: %s\n", buffer);
    
    // Step 5: Send response back to the client
    sendto(server_fd, hello, strlen(hello), 
           MSG_CONFIRM, (const struct sockaddr *)&client_addr, len);
    printf("Hello message sent to client\n");
    
    // Step 6: Clean up
    close(server_fd);
    
    return 0;
}
