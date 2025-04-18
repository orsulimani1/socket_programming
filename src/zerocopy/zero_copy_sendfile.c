// Zero-Copy Socket Implementation Example with sendfile()
// This example shows how to efficiently transfer a file over a socket without
// copying data between user and kernel space

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

// Function to handle errors
void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    // Check if filename is provided
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file_to_send>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // Step 1: Open the file to be sent
    int file_fd = open(argv[1], O_RDONLY);
    if (file_fd == -1) {
        error("Error opening file");
    }
    
    // Get file size
    off_t file_size = lseek(file_fd, 0, SEEK_END);
    lseek(file_fd, 0, SEEK_SET);  // Reset file pointer to beginning
    
    if (file_size == -1) {
        error("Error getting file size");
    }
    
    printf("File size: %ld bytes\n", (long)file_size);
    
    // Step 2: Create TCP socket
    int server_fd;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        error("Error creating socket");
    }
    
    // Step 3: Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        error("Error setting socket options");
    }
    
    // Step 4: Set up server address
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    // Step 5: Bind socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        error("Error binding socket");
    }
    
    // Step 6: Listen for connections
    if (listen(server_fd, 1) == -1) {
        error("Error listening");
    }
    
    printf("Server listening on port %d. Ready to send file using zero-copy.\n", PORT);
    
    // Step 7: Accept connection
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_fd == -1) {
        error("Error accepting connection");
    }
    
    printf("Connection accepted from %s:%d\n", 
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    
    // Step 8: Send file size to client first (to let them know how much to expect)
    char size_buffer[32];
    sprintf(size_buffer, "%ld", (long)file_size);
    if (send(client_fd, size_buffer, strlen(size_buffer), 0) == -1) {
        error("Error sending file size");
    }
    
    // Small delay to ensure the client is ready to receive the file
    sleep(1);
    
    // Step 9: Use sendfile() for zero-copy file transfer
    printf("Starting zero-copy file transfer...\n");
    
    off_t offset = 0;
    ssize_t sent_bytes = 0;
    ssize_t remaining_bytes = file_size;
    
    while (offset < file_size) {
        // sendfile() transfers data directly from file descriptor to socket
        // without copying between kernel and user space
        sent_bytes = sendfile(client_fd, file_fd, &offset, remaining_bytes);
        
        if (sent_bytes == -1) {
            error("Error in sendfile()");
        }
        
        if (sent_bytes == 0) {
            break;  // End of file
        }
        
        remaining_bytes -= sent_bytes;
        printf("Progress: %.2f%%\r", (100.0 * offset) / file_size);
        fflush(stdout);
    }
    
    printf("\nFile transfer complete. Sent %ld bytes using zero-copy.\n", (long)offset);
    
    // Step 10: Clean up
    close(file_fd);
    close(client_fd);
    close(server_fd);
    
    return 0;
}
