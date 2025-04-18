/**
 * @file secure_command_server.c
 * @brief Secure command & control system using TLS/SSL
 * 
 * This example demonstrates a secure remote command and control system
 * for managing industrial equipment using TLS/SSL encryption.
 * 
 * Note: This example requires OpenSSL development libraries.
 * To compile with gcc directly: 
 *   gcc -o secure_command_server secure_command_server.c -lssl -lcrypto
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include "socket_utils.h"
#include "error_handling.h"
#include "config.h"

// Check if TLS is enabled in configuration
#ifdef ENABLE_TLS

#include <openssl/ssl.h>
#include <openssl/err.h>

#define COMMAND_PORT 8443
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define CERT_FILE "server.crt"
#define KEY_FILE "server.key"
#define CA_FILE "ca.crt"  // For client certificate verification

// Flag for graceful shutdown
static volatile int keep_running = 1;

// Command handler structure
typedef struct {
    const char *command;
    const char *description;
    const char *(*handler)(const char *args);
} command_handler_t;

// SSL context and client information
typedef struct {
    SSL *ssl;
    int socket;
    pthread_t thread;
    char client_ip[INET_ADDRSTRLEN];
    int client_port;
} client_info_t;

// Client connections
client_info_t clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Signal handler for graceful shutdown
void handle_signal(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    keep_running = 0;
}

// Initialize OpenSSL
void init_openssl() {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    ERR_load_crypto_strings();
}

// Clean up OpenSSL
void cleanup_openssl() {
    ERR_free_strings();
    EVP_cleanup();
}

// Create SSL context
SSL_CTX *create_ssl_context() {
    const SSL_METHOD *method = TLS_server_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        FATAL("Failed to create SSL context");
    }
    
    // Set minimum TLS version (TLS 1.2)
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    
    // Set up strong ciphers
    if (SSL_CTX_set_cipher_list(ctx, "HIGH:!aNULL:!MD5:!RC4") != 1) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        FATAL("Failed to set cipher list");
    }
    
    return ctx;
}

// Configure SSL context with certificates
void configure_ssl_context(SSL_CTX *ctx) {
    // Load server certificate
    if (SSL_CTX_use_certificate_file(ctx, CERT_FILE, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        FATAL("Failed to load server certificate");
    }
    
    // Load private key
    if (SSL_CTX_use_PrivateKey_file(ctx, KEY_FILE, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        FATAL("Failed to load server private key");
    }
    
    // Verify private key
    if (SSL_CTX_check_private_key(ctx) != 1) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        FATAL("Server private key does not match certificate");
    }
    
    // Set up client certificate verification
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
    
    // Load CA certificate for client verification
    if (SSL_CTX_load_verify_locations(ctx, CA_FILE, NULL) != 1) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        FATAL("Failed to load CA certificate");
    }
}

// Example command handlers

// Status command
const char *handle_status(const char *args) {
    return "System status: ONLINE\nTemperature: 72.5°F\nPressure: 1013.2 hPa\nHumidity: 45.3%";
}

// Reboot command
const char *handle_reboot(const char *args) {
    return "Initiating system reboot sequence...";
    // In a real system, this would trigger a reboot
}

// Shutdown command
const char *handle_shutdown(const char *args) {
    return "Initiating system shutdown sequence...";
    // In a real system, this would trigger a shutdown
}

// Set parameter command
const char *handle_set(const char *args) {
    if (!args || !*args) {
        return "Error: Parameter name and value required";
    }
    
    static char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "Setting parameter: %s", args);
    return response;
}

// Get parameter command
const char *handle_get(const char *args) {
    if (!args || !*args) {
        return "Error: Parameter name required";
    }
    
    static char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "Parameter %s = 42.0", args);
    return response;
}

// Help command
const char *handle_help(const char *args) {
    static const char *help_text =
        "Available commands:\n"
        "  status           - Show system status\n"
        "  reboot           - Reboot the system\n"
        "  shutdown         - Shutdown the system\n"
        "  set <param> <val>- Set parameter value\n"
        "  get <param>      - Get parameter value\n"
        "  help             - Show this help text\n"
        "  quit             - Close connection";
    
    return help_text;
}

// Command table
command_handler_t command_handlers[] = {
    {"status",    "Show system status",       handle_status},
    {"reboot",    "Reboot the system",        handle_reboot},
    {"shutdown",  "Shutdown the system",      handle_shutdown},
    {"set",       "Set parameter value",      handle_set},
    {"get",       "Get parameter value",      handle_get},
    {"help",      "Show help text",           handle_help},
    {NULL,        NULL,                       NULL}  // End marker
};

// Process and execute a command
const char *process_command(const char *command_line) {
    static char response[BUFFER_SIZE];
    
    // Skip leading whitespace
    while (*command_line && isspace(*command_line)) {
        command_line++;
    }
    
    if (!*command_line) {
        return ""; // Empty command, return empty response
    }
    
    // Extract command and arguments
    char cmd[BUFFER_SIZE];
    const char *args = NULL;
    
    // Find first space to separate command from args
    const char *space = strchr(command_line, ' ');
    if (space) {
        size_t cmd_len = space - command_line;
        if (cmd_len >= sizeof(cmd)) {
            cmd_len = sizeof(cmd) - 1;
        }
        
        strncpy(cmd, command_line, cmd_len);
        cmd[cmd_len] = '\0';
        
        // Skip whitespace after command
        args = space + 1;
        while (*args && isspace(*args)) {
            args++;
        }
    } else {
        strncpy(cmd, command_line, sizeof(cmd) - 1);
        cmd[sizeof(cmd) - 1] = '\0';
    }
    
    // Handle special 'quit' command
    if (strcmp(cmd, "quit") == 0) {
        return "Closing connection...";
    }
    
    // Search for command handler
    for (int i = 0; command_handlers[i].command != NULL; i++) {
        if (strcmp(cmd, command_handlers[i].command) == 0) {
            return command_handlers[i].handler(args);
        }
    }
    
    // Command not found
    snprintf(response, sizeof(response), "Unknown command: %s\nType 'help' for available commands", cmd);
    return response;
}

// Handle client connections
void *handle_client(void *arg) {
    client_info_t *client = (client_info_t *)arg;
    SSL *ssl = client->ssl;
    int client_sock = client->socket;
    
    printf("Handling client %s:%d\n", client->client_ip, client->client_port);
    
    // SSL handshake
    if (SSL_accept(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        printf("SSL handshake failed with client %s:%d\n", 
            client->client_ip, client->client_port);
        SSL_free(ssl);
        close(client_sock);
        
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket == client_sock) {
                clients[i].socket = -1;
                clients[i].ssl = NULL;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
        
        return NULL;
    }
    
    // Verify client certificate
    X509 *client_cert = SSL_get_peer_certificate(ssl);
    if (client_cert) {
        // Get client identity information
        char subject_name[256];
        X509_NAME_oneline(X509_get_subject_name(client_cert), subject_name, sizeof(subject_name));
        printf("Client certificate subject: %s\n", subject_name);
        
        // In a real system, additional validation would be done here
        X509_free(client_cert);
    } else {
        printf("No client certificate presented (should not happen with our verification settings)\n");
        SSL_free(ssl);
        close(client_sock);
        
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket == client_sock) {
                clients[i].socket = -1;
                clients[i].ssl = NULL;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
        
        return NULL;
    }
    
    // Send welcome message
    const char *welcome = "Welcome to the Secure Command Server\r\n"
                        "Type 'help' for available commands, 'quit' to exit\r\n"
                        "> ";
    SSL_write(ssl, welcome, strlen(welcome));
    
    // Client command processing loop
    char buffer[BUFFER_SIZE];
    int bytes;
    
    while ((bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1)) > 0) {
        // Null-terminate the received data
        buffer[bytes] = '\0';
        
        // Remove trailing newline
        if (bytes > 0 && (buffer[bytes-1] == '\n' || buffer[bytes-1] == '\r')) {
            buffer[--bytes] = '\0';
        }
        if (bytes > 0 && (buffer[bytes-1] == '\n' || buffer[bytes-1] == '\r')) {
            buffer[--bytes] = '\0';
        }
        
        printf("Client %s:%d sent command: %s\n", 
            client->client_ip, client->client_port, buffer);
        
        // Process command
        const char *response = process_command(buffer);
        
        // Send response with prompt
        char full_response[BUFFER_SIZE + 8];
        snprintf(full_response, sizeof(full_response), "%s\r\n> ", response);
        
        SSL_write(ssl, full_response, strlen(full_response));
        
        // Check for quit command
        if (strncmp(buffer, "quit", 4) == 0) {
            break;
        }
    }
    
    // Check for errors
    if (bytes < 0) {
        int ssl_error = SSL_get_error(ssl, bytes);
        if (ssl_error != SSL_ERROR_ZERO_RETURN) {
            printf("SSL_read error with client %s:%d: %d\n", 
                client->client_ip, client->client_port, ssl_error);
        }
    }
    
    // Clean up
    printf("Client %s:%d disconnected\n", client->client_ip, client->client_port);
    SSL_free(ssl);
    close(client_sock);
    
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == client_sock) {
            clients[i].socket = -1;
            clients[i].ssl = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    
    return NULL;
}

int main() {
    // Set up signal handling for graceful shutdown
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // Initialize OpenSSL
    init_openssl();
    
    // Create SSL context
    SSL_CTX *ctx = create_ssl_context();
    configure_ssl_context(ctx);
    
    // Create server socket
    int server_fd = create_tcp_socket(1, 0);  // With SO_REUSEADDR, blocking mode
    if (server_fd < 0) {
        SSL_CTX_free(ctx);
        cleanup_openssl();
        FATAL_ERRNO("Failed to create socket");
    }
    
    // Prepare address structure
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(COMMAND_PORT);
    
    // Bind socket to address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        close(server_fd);
        SSL_CTX_free(ctx);
        cleanup_openssl();
        FATAL_ERRNO("Failed to bind to port %d", COMMAND_PORT);
    }
    
    // Listen for incoming connections
    if (listen(server_fd, 5) < 0) {
        close(server_fd);
        SSL_CTX_free(ctx);
        cleanup_openssl();
        FATAL_ERRNO("Failed to listen on socket");
    }
    
    printf("Secure command server listening on port %d\n", COMMAND_PORT);
    printf("Press Ctrl+C to shut down\n");
    
    // Initialize client array
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = -1;
        clients[i].ssl = NULL;
    }
    
    // Main server loop
    while (keep_running) {
        // Accept connections
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_sock = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            if (errno == EINTR) {
                // Interrupted by signal, check if we should keep running
                continue;
            }
            
            LOG_ERRNO("Accept failed");
            continue;
        }
        
        // Find available slot for client
        int slot = -1;
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket < 0) {
                slot = i;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
        
        if (slot < 0) {
            // No slots available, reject connection
            close(client_sock);
            printf("Rejected connection: maximum clients reached\n");
            continue;
        }
        
        // Create new SSL structure
        SSL *ssl = SSL_new(ctx);
        if (!ssl) {
            close(client_sock);
            printf("Failed to create SSL structure\n");
            continue;
        }
        
        // Set socket for SSL
        if (SSL_set_fd(ssl, client_sock) != 1) {
            SSL_free(ssl);
            close(client_sock);
            printf("Failed to set SSL file descriptor\n");
            continue;
        }
        
        // Store client information
        pthread_mutex_lock(&clients_mutex);
        clients[slot].socket = client_sock;
        clients[slot].ssl = ssl;
        inet_ntop(AF_INET, &client_addr.sin_addr, clients[slot].client_ip, INET_ADDRSTRLEN);
        clients[slot].client_port = ntohs(client_addr.sin_port);
        pthread_mutex_unlock(&clients_mutex);
        
        printf("New connection from %s:%d\n", 
            clients[slot].client_ip, clients[slot].client_port);
        
        // Create thread to handle client
        if (pthread_create(&clients[slot].thread, NULL, handle_client, &clients[slot]) != 0) {
            pthread_mutex_lock(&clients_mutex);
            clients[slot].socket = -1;
            clients[slot].ssl = NULL;
            pthread_mutex_unlock(&clients_mutex);
            
            SSL_free(ssl);
            close(client_sock);
            printf("Failed to create thread for client\n");
            continue;
        }
        
        // Detach thread
        pthread_detach(clients[slot].thread);
    }
    
    // Clean up
    printf("Shutting down secure command server...\n");
    
    // Close all client connections
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket >= 0) {
            SSL_free(clients[i].ssl);
            close(clients[i].socket);
            clients[i].socket = -1;
            clients[i].ssl = NULL;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    
    // Clean up server
    close(server_fd);
    SSL_CTX_free(ctx);
    cleanup_openssl();
    pthread_mutex_destroy(&clients_mutex);
    
    printf("Server shutdown complete\n");
    
    return 0;
}

#else  // !ENABLE_TLS

// Simplified version for systems without TLS support
int main() {
    printf("This secure command server requires TLS/SSL support.\n");
    printf("Please enable TLS in config.h and install OpenSSL development libraries.\n");
    return 1;
}

#endif  // ENABLE_TLS