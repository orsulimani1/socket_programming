
// Zero-Copy Socket Implementation Client Example
// This client receives a file sent by the zero-copy server

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 4096  // Larger buffer for receiving

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    // Check if server IP and output filename are provided
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <output_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // Step 1: Create TCP socket
    int sock_fd;
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        error("Error creating socket");
    }
    
    // Step 2: Set up server address
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    // Convert IP address from text to binary
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        error("Invalid address or address not supported");
    }
    
    // Step 3: Connect to server
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        error("Connection failed");
    }
    
    printf("Connected to server. Waiting to receive file...\n");
    
    // Step 4: Receive file size first
    char size_buffer[32] = {0};
    if (recv(sock_fd, size_buffer, sizeof(size_buffer) - 1, 0) == -1) {
        error("Error receiving file size");
    }
    
    off_t file_size = atol(size_buffer);
    printf("File size to receive: %ld bytes\n", (long)file_size);
    
    // Step 5: Open output file for writing
    int file_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd == -1) {
        error("Error opening output file");
    }
    
    // Step 6: Receive and write file data
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = 0;
    ssize_t bytes_written = 0;
    ssize_t total_received = 0;
    
    while (total_received < file_size) {
        // Receive data from server
        bytes_received = recv(sock_fd, buffer, BUFFER_SIZE, 0);
        
        if (bytes_received == -1) {
            error("Error receiving data");
        }
        
        if (bytes_received == 0) {
            break;  // Connection closed by server
        }
        
        // Write received data to file
        bytes_written = write(file_fd, buffer, bytes_received);
        if (bytes_written != bytes_received) {
            error("Error writing to file");
        }
        
        total_received += bytes_received;
        printf("Progress: %.2f%%\r", (100.0 * total_received) / file_size);
        fflush(stdout);
    }
    
    printf("\nFile transfer complete. Received %ld bytes.\n", (long)total_received);
    
    // Step 7: Clean up
    close(file_fd);
    close(sock_fd);
    
    return 0;
}
