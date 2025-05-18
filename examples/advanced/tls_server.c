/**
 * @file tls_server.c
 * @brief Example TLS/SSL secure socket server
 * 
 * This example demonstrates how to implement a secure socket server
 * using OpenSSL for TLS/SSL encryption.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "../../include/socket_utils.h"
#include "../../include/error_handling.h"
#include "../../include/config.h"

// Default port for HTTPS
#define DEFAULT_PORT 8443
#define BUFFER_SIZE 1024

// Certificate and key files
#define CERT_FILE "/tmp/server.crt"
#define KEY_FILE "/tmp/server.key"

// Initialize OpenSSL
void init_openssl() {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    ERR_load_crypto_strings();
}

// Clean up OpenSSL resources
void cleanup_openssl() {
    EVP_cleanup();
    ERR_free_strings();
}

// Create new SSL context
SSL_CTX *create_ssl_context() {
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    // Use TLS method
    method = TLS_server_method();
    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

// Configure SSL context with certificate and key
void configure_ssl_context(SSL_CTX *ctx) {
    // Load certificate and private key
    if (SSL_CTX_use_certificate_file(ctx, CERT_FILE, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, KEY_FILE, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    // Verify private key
    if (!SSL_CTX_check_private_key(ctx)) {
        fprintf(stderr, "Private key does not match the public certificate\n");
        exit(EXIT_FAILURE);
    }
    
    // Set minimum TLS version
    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    
    // Use only strong ciphers
    if (!SSL_CTX_set_cipher_list(ctx, "HIGH:!aNULL:!MD5:!RC4")) {
        fprintf(stderr, "Error setting cipher list\n");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    printf("%s\n", argv[0]);
    // Parse command line arguments for port
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    // Initialize OpenSSL
    init_openssl();
    
    // Create and configure SSL context
    SSL_CTX *ssl_ctx = create_ssl_context();
    configure_ssl_context(ssl_ctx);
    
    printf("TLS Server Example\n");
    printf("Using OpenSSL version: %s\n", OpenSSL_version(OPENSSL_VERSION));
    
    // Create TCP socket
    int server_fd = create_tcp_socket(1, 0);  // With SO_REUSEADDR, blocking mode
    if (server_fd < 0) {
        FATAL_ERRNO("Failed to create socket");
    }
    
    // Prepare address structure
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    // Bind socket to port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(server_fd);
        FATAL_ERRNO("Failed to bind to port %d", port);
    }
    
    // Start listening for connections
    if (listen(server_fd, 5) < 0) {
        close(server_fd);
        FATAL_ERRNO("Failed to listen on socket");
    }
    
    printf("Server listening on port %d\n", port);
    printf("Press Ctrl+C to stop the server\n");
    
    // Main server loop
    while (1) {
        // Accept client connection
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        
        if (client_fd < 0) {
            perror("Unable to accept connection");
            continue;
        }
        
        // Get client IP and port
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);
        
        printf("Client connected: %s:%d\n", client_ip, client_port);
        
        // Create new SSL structure
        SSL *ssl = SSL_new(ssl_ctx);
        if (!ssl) {
            ERR_print_errors_fp(stderr);
            close(client_fd);
            continue;
        }
        
        // Attach SSL to socket
        SSL_set_fd(ssl, client_fd);
        
        // Perform SSL handshake
        if (SSL_accept(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            close(client_fd);
            printf("SSL handshake failed with %s:%d\n", client_ip, client_port);
            continue;
        }
        
        printf("SSL connection established with %s:%d using %s\n", 
            client_ip, client_port, SSL_get_cipher(ssl));
        
        // Show SSL certificate info
        X509 *cert = SSL_get_peer_certificate(ssl);
        if (cert) {
            printf("Client certificate:\n");
            X509_NAME *subject_name = X509_get_subject_name(cert);
            char subject_buf[256];
            X509_NAME_oneline(subject_name, subject_buf, sizeof(subject_buf));
            printf("  Subject: %s\n", subject_buf);
            X509_free(cert);
        } else {
            printf("Client did not provide a certificate\n");
        }
        
        // Send welcome message
        const char *welcome_msg = "Welcome to the TLS Server Example!\r\n"
                                "Type 'quit' to close connection\r\n";
        SSL_write(ssl, welcome_msg, strlen(welcome_msg));
        
        // Handle client communication
        char buffer[BUFFER_SIZE];
        int bytes;
        
        while ((bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1)) > 0) {
            // Null-terminate the received data
            buffer[bytes] = '\0';
            
            // Print received data
            printf("Received from %s:%d: %s", client_ip, client_port, buffer);
            
            // Echo back to client
            SSL_write(ssl, buffer, bytes);
            
            // Check for quit command
            if (strncmp(buffer, "quit", 4) == 0) {
                printf("Client requested to quit\n");
                break;
            }
        }
        
        // Handle read errors
        if (bytes < 0) {
            ERR_print_errors_fp(stderr);
        }
        
        // Close SSL connection
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_fd);
        
        printf("Connection with %s:%d closed\n", client_ip, client_port);
    }
    
    // Clean up
    close(server_fd);
    SSL_CTX_free(ssl_ctx);
    cleanup_openssl();
    
    return 0;
}