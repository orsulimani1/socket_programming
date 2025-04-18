// Unix Domain Socket Client Example

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/uds_socket"
#define BUFFER_SIZE 1024

int main() {
    int sock;
    struct sockaddr_un server_addr;
    char buffer[BUFFER_SIZE];
    
    // Step 1: Create Unix domain socket file descriptor
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("Unix domain socket created successfully\n");
    
    // Step 2: Set up server address structure to connect to
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);
    
    // Step 3: Connect to the server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("Connected to server\n");
    
    // Step 4: Send message to server
    char *message = "Hello from UDS client";
    write(sock, message, strlen(message));
    printf("Message sent to server\n");
    
    // Step 5: Read response from server
    ssize_t bytes_read = read(sock, buffer, BUFFER_SIZE - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';  // Null terminate
        printf("Message from server: %s\n", buffer);
    }
    
    // Step 6: Clean up
    close(sock);
    
    return 0;
}