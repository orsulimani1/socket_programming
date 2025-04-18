// TCP Socket Client Example

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char *hello = "Hello from client";
    char buffer[BUFFER_SIZE] = {0};
    
    // Step 1: Create socket file descriptor
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("Socket created successfully\n");
    
    // Step 2: Set up server address structure to connect to
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    // Convert IPv4 address from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address or address not supported");
        close(sock);
        exit(EXIT_FAILURE);
    }
    
    // Step 3: Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("Connected to server\n");
    
    // Step 4: Send message to server
    send(sock, hello, strlen(hello), 0);
    printf("Hello message sent to server\n");
    
    // Step 5: Read response from server
    int valread = read(sock, buffer, BUFFER_SIZE);
    printf("Message from server: %s\n", buffer);
    
    // Step 6: Clean up
    close(sock);
    
    return 0;
}