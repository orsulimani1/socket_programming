
// UDP Socket Client Example

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char *hello = "Hello from UDP client";
    char buffer[BUFFER_SIZE] = {0};
    
    // Step 1: Create UDP socket file descriptor
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("My error is: %s", strerror(errno));
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("UDP socket created successfully\n");
    
    // Step 2: Set up server address structure to communicate with
    memset(&server_addr, 0, sizeof(server_addr));
    
    server_addr.sin_family = AF_INET;  // IPv4
    server_addr.sin_port = htons(PORT);  // Convert to network byte order
    
    // Convert IPv4 address from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Invalid address or address not supported");
        close(sock);
        exit(EXIT_FAILURE);
    }
    
    // Step 3: Send message to server (no connect needed for UDP)
    sendto(sock, hello, strlen(hello), 
           MSG_CONFIRM, (const struct sockaddr *)&server_addr, sizeof(server_addr));
    printf("Hello message sent to server\n");
    
    // Step 4: Receive response from server
    int len = sizeof(server_addr);
    int n = recvfrom(sock, buffer, BUFFER_SIZE, 
                    MSG_WAITALL, (struct sockaddr *)&server_addr, &len);
    buffer[n] = '\0';  // Null terminate the received data
    printf("Message from server: %s\n", buffer);
    
    // Step 5: Clean up
    close(sock);
    
    return 0;
}