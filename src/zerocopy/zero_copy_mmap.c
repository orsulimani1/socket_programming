// Alternative Zero-Copy Implementation Using mmap() and splice()

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8090
#define BUFFER_SIZE 4096

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Using mmap() for Zero-Copy File Sending
int send_file_with_mmap(int client_fd, const char *filename) {
    int file_fd;
    struct stat file_stat;
    void *file_memory;
    
    // Open the file
    if ((file_fd = open(filename, O_RDONLY)) == -1) {
        error("Error opening file");
    }
    
    // Get file information
    if (fstat(file_fd, &file_stat) == -1) {
        error("Error getting file stats");
    }
    
    // Map file into memory
    file_memory = mmap(NULL, file_stat.st_size, PROT_READ, MAP_PRIVATE, file_fd, 0);
    if (file_memory == MAP_FAILED) {
        error("Error mapping file into memory");
    }
    
    // Send the file size first
    char size_buffer[32];
    sprintf(size_buffer, "%ld", (long)file_stat.st_size);
    if (send(client_fd, size_buffer, strlen(size_buffer), 0) == -1) {
        error("Error sending file size");
    }
    
    // Give client time to prepare
    sleep(1);
    
    printf("Starting mmap-based zero-copy transfer...\n");
    
    // Send the mapped memory directly to the socket
    // Since we're using writev() with the mapped memory, 
    // the kernel doesn't need to copy the data to a temporary buffer
    ssize_t bytes_sent = 0;
    size_t remaining = file_stat.st_size;
    size_t offset = 0;
    
    while (remaining > 0) {
        bytes_sent = write(client_fd, file_memory + offset, remaining);
        
        if (bytes_sent == -1) {
            error("Error writing to socket");
        }
        
        remaining -= bytes_sent;
        offset += bytes_sent;
        
        printf("Progress: %.2f%%\r", (100.0 * offset) / file_stat.st_size);
        fflush(stdout);
    }
    
    printf("\nFile transfer complete. Sent %ld bytes using mmap zero-copy.\n", 
           (long)file_stat.st_size);
    
    // Clean up
    munmap(file_memory, file_stat.st_size);
    close(file_fd);
    
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file_to_send>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // Create socket, bind, listen and accept connection
    // (Same steps as in the sendfile example)
    
    int server_fd;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        error("Error creating socket");
    }
    
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        error("Error setting socket options");
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        error("Error binding socket");
    }
    
    if (listen(server_fd, 1) == -1) {
        error("Error listening");
    }
    
    printf("Server listening on port %d. Ready to send file using mmap zero-copy.\n", PORT);
    
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_fd == -1) {
        error("Error accepting connection");
    }
    
    printf("Connection accepted from %s:%d\n", 
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    
    // Send file using mmap-based zero-copy
    send_file_with_mmap(client_fd, argv[1]);
    
    // Clean up
    close(client_fd);
    close(server_fd);
    
    return 0;
}