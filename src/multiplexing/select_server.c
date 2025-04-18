// Socket Multiplexing with select() Example

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>

#define PORT 8080
#define MAX_CLIENTS 30
#define BUFFER_SIZE 1024

int main() {
    int server_fd, new_socket, client_sockets[MAX_CLIENTS], max_clients = MAX_CLIENTS;
    int activity, i, valread, sd;
    int max_sd;
    struct sockaddr_in address;
    
    char buffer[BUFFER_SIZE];
    char *welcome_message = "Welcome to the multiplexed server\n";
    
    // Set of socket descriptors for select()
    fd_set readfds;
    
    // Initialize all client sockets as invalid (value -1)
    for (i = 0; i < max_clients; i++) {
        client_sockets[i] = -1;
    }
    
    // Step 1: Create TCP socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("Socket created successfully\n");
    
    // Step 2: Set socket options to allow reuse of address and port
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    // Step 3: Set up server address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    // Step 4: Bind socket to the specified IP and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Socket bound to port %d\n", PORT);
    
    // Step 5: Start listening for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    printf("Server is listening...\n");
    
    // Accept incoming connections
    socklen_t addrlen = sizeof(address);
    printf("Waiting for connections...\n");
    
    while (1) {
        // Step 6: Clear the socket set
        FD_ZERO(&readfds);
        
        // Step 7: Add server socket to the set
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;
        
        // Step 8: Add child sockets to the set
        for (i = 0; i < max_clients; i++) {
            sd = client_sockets[i];
            
            // If valid socket descriptor then add to read list
            if (sd > 0) {
                FD_SET(sd, &readfds);
            }
            
            // Keep track of highest file descriptor number
            if (sd > max_sd) {
                max_sd = sd;
            }
        }
        
        // Step 9: Wait for activity on any of the sockets
        // Timeout is NULL, so wait indefinitely
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        
        if ((activity < 0)) {
            perror("select error");
            exit(EXIT_FAILURE);
        }
        
        // Step 10: Check if there's activity on the server socket
        // If yes, it's an incoming connection
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
                perror("Accept failed");
                exit(EXIT_FAILURE);
            }
            
            printf("New connection, socket fd: %d, IP: %s, port: %d\n",
                   new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
            
            // Step 11: Send welcome message
            if (send(new_socket, welcome_message, strlen(welcome_message), 0) != strlen(welcome_message)) {
                perror("Send failed");
            }
            printf("Welcome message sent successfully\n");
            
            // Step 12: Add new socket to array of sockets
            for (i = 0; i < max_clients; i++) {
                if (client_sockets[i] == -1) {  // If position is empty
                    client_sockets[i] = new_socket;
                    printf("Adding to list of sockets at index %d\n", i);
                    break;
                }
            }
        }
        
        // Step 13: Check for I/O operation on client sockets
        for (i = 0; i < max_clients; i++) {
            sd = client_sockets[i];
            
            if (FD_ISSET(sd, &readfds)) {
                // Check if it was for closing, and read the incoming message
                memset(buffer, 0, BUFFER_SIZE);
                if ((valread = read(sd, buffer, BUFFER_SIZE - 1)) == 0) {
                    // Client disconnected
                    getpeername(sd, (struct sockaddr*)&address, &addrlen);
                    printf("Client disconnected, IP: %s, port: %d\n",
                           inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                    
                    // Close the socket and mark as invalid in list
                    close(sd);
                    client_sockets[i] = -1;
                } else {
                    // Process the data from client
                    buffer[valread] = '\0';
                    printf("Received message from client %d: %s\n", i, buffer);
                    
                    // Echo back the message
                    send(sd, buffer, strlen(buffer), 0);
                }
            }
        }
    }
    
    return 0;
}