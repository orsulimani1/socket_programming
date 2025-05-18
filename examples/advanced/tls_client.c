/**
 * @file tls_client.c
 * @brief Example TLS/SSL secure socket client
 * 
 * This example demonstrates how to implement a secure socket client
 * using OpenSSL for TLS/SSL encryption. It connects to the tls_server
 * example.
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

// Default settings
#define DEFAULT_SERVER "127.0.0.1"
#define DEFAULT_PORT 8443
#define BUFFER_SIZE 1024

// Certificate file (used for verification)
#define CERT_FILE "/tmp/server.crt"

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

// Create new SSL context for client
SSL_CTX *create_ssl_context() {
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    // Use TLS client method
    method = TLS_client_method();
    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

// Configure SSL context with verification settings
void configure_ssl_context(SSL_CTX *ctx) {
    // Set verification mode
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    
    // Load trusted certificates (server certificate)
    if (SSL_CTX_load_verify_locations(ctx, CERT_FILE, NULL) != 1) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    
    // Set minimum TLS version
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    
    // Use only strong ciphers
    if (!SSL_CTX_set_cipher_list(ctx, "HIGH:!aNULL:!MD5:!RC4")) {
        fprintf(stderr, "Error setting cipher list\n");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    const char *server = DEFAULT_SERVER;
    int port = DEFAULT_PORT;
    
    // Parse command line arguments for server and port
    if (argc > 1) {
        server = argv[1];
    }
    
    if (argc > 2) {
        port = atoi(argv[2]);
    }
    
    // Initialize OpenSSL
    init_openssl();
    
    // Create and configure SSL context
    SSL_CTX *ssl_ctx = create_ssl_context();
    configure_ssl_context(ssl_ctx);
    
    printf("TLS Client Example\n");
    printf("Using OpenSSL version: %s\n", OpenSSL_version(OPENSSL_VERSION));
    printf("Connecting to %s:%d\n", server, port);
    
    // Create TCP socket
    int sock_fd = create_tcp_socket(0, 0);  // Without SO_REUSEADDR, blocking mode
    if (sock_fd < 0) {
        FATAL_ERRNO("Failed to create socket");
    }
    
    // Prepare server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    // Convert IP address from text to binary form
    if (inet_pton(AF_INET, server, &server_addr.sin_addr) <= 0) {
        close(sock_fd);
        FATAL_ERRNO("Invalid address: %s", server);
    }
    
    // Connect to server
    printf("Attempting connection to server...\n");
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sock_fd);
        FATAL_ERRNO("Connection failed");
    }
    
    // Create new SSL structure
    SSL *ssl = SSL_new(ssl_ctx);
    if (!ssl) {
        ERR_print_errors_fp(stderr);
        close(sock_fd);
        SSL_CTX_free(ssl_ctx);
        exit(EXIT_FAILURE);
    }
    
    // Attach SSL to socket
    SSL_set_fd(ssl, sock_fd);
    
    // Set SNI (Server Name Indication)
    SSL_set_tlsext_host_name(ssl, server);
    
    // Perform SSL handshake
    printf("Initiating TLS handshake...\n");
    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(sock_fd);
        SSL_CTX_free(ssl_ctx);
        exit(EXIT_FAILURE);
    }
    
    printf("SSL connection established using %s\n", SSL_get_cipher(ssl));
    
    // Verify server certificate
    X509 *cert = SSL_get_peer_certificate(ssl);
    if (cert) {
        printf("Server certificate:\n");
        
        // Get subject name
        X509_NAME *subject_name = X509_get_subject_name(cert);
        char subject_buf[256];
        X509_NAME_oneline(subject_name, subject_buf, sizeof(subject_buf));
        printf("  Subject: %s\n", subject_buf);
        
        // Get issuer name
        X509_NAME *issuer_name = X509_get_issuer_name(cert);
        char issuer_buf[256];
        X509_NAME_oneline(issuer_name, issuer_buf, sizeof(issuer_buf));
        printf("  Issuer: %s\n", issuer_buf);
        
        X509_free(cert);
        
        // Verify certificate chain
        long verify_result = SSL_get_verify_result(ssl);
        if (verify_result != X509_V_OK) {
            printf("Warning: Server certificate verification failed: %s\n", 
                   X509_verify_cert_error_string(verify_result));
            printf("Continuing anyway (for testing purposes)\n");
        } else {
            printf("Server certificate verified successfully\n");
        }
    } else {
        printf("Error: Server did not present a certificate\n");
        SSL_free(ssl);
        close(sock_fd);
        SSL_CTX_free(ssl_ctx);
        exit(EXIT_FAILURE);
    }
    
    // Receive welcome message
    char buffer[BUFFER_SIZE];
    int bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("Server says:\n%s", buffer);
    }
    
    // Interactive mode - send messages until "quit"
    printf("\nEnter messages to send to the server (type 'quit' to exit):\n");
    
    while (1) {
        printf("> ");
        fflush(stdout);
        
        // Read input from user
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;
        }
        
        // Send message to server
        SSL_write(ssl, buffer, strlen(buffer));
        
        // Check if we should quit
        if (strncmp(buffer, "quit", 4) == 0) {
            printf("Closing connection.\n");
            break;
        }
        
        // Receive response
        bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            printf("Server response: %s", buffer);
        } else if (bytes == 0) {
            printf("Connection closed by server\n");
            break;
        } else {
            ERR_print_errors_fp(stderr);
            break;
        }
    }
    
    // Clean up
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sock_fd);
    SSL_CTX_free(ssl_ctx);
    cleanup_openssl();
    
    printf("Connection terminated.\n");
    return 0;
}