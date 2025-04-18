/**
 * @file socket_encryption.c
 * @brief Example of socket communication with data encryption
 *
 * This example demonstrates how to implement encrypted socket
 * communication using OpenSSL's EVP API for encryption and decryption.
 * It shows a client-server architecture with AES-256-GCM encryption.
 *
 * Compile with: gcc -o socket_encryption socket_encryption.c -lssl -lcrypto
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include "../../include/socket_utils.h"
#include "../../include/error_handling.h"
#include "../../include/config.h"

#define PORT 8888
#define BUFFER_SIZE 2048
#define KEY_SIZE 32   // 256 bits for AES-256
#define IV_SIZE 12    // 96 bits for GCM mode
#define TAG_SIZE 16   // 128 bits for authentication tag
#define AAD_SIZE 16   // Size of additional authenticated data

// Structure for encrypted message
typedef struct {
    unsigned char iv[IV_SIZE];       // Initialization vector
    unsigned char aad[AAD_SIZE];     // Additional authenticated data
    unsigned char tag[TAG_SIZE];     // Authentication tag
    size_t ciphertext_len;           // Length of ciphertext
    unsigned char ciphertext[BUFFER_SIZE]; // Encrypted data
} encrypted_message;

// Encryption key (should be securely exchanged in a real application)
unsigned char key[KEY_SIZE];

// Function to print OpenSSL errors
void print_openssl_errors() {
    unsigned long err;
    while ((err = ERR_get_error())) {
        char err_msg[256];
        ERR_error_string_n(err, err_msg, sizeof(err_msg));
        fprintf(stderr, "OpenSSL error: %s\n", err_msg);
    }
}

// Generate random key
void generate_key() {
    if (RAND_bytes(key, KEY_SIZE) != 1) {
        print_openssl_errors();
        exit(EXIT_FAILURE);
    }
    
    printf("Generated encryption key: ");
    for (int i = 0; i < KEY_SIZE; i++) {
        printf("%02x", key[i]);
    }
    printf("\n");
}

// Encrypt data using AES-256-GCM
int encrypt_data(const unsigned char *plaintext, size_t plaintext_len,
                const unsigned char *aad, size_t aad_len,
                const unsigned char *key, const unsigned char *iv,
                unsigned char *ciphertext, unsigned char *tag) {
    
    EVP_CIPHER_CTX *ctx;
    int len;
    int ciphertext_len;
    int ret;
    
    // Create and initialize the context
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        print_openssl_errors();
        return -1;
    }
    
    // Initialize the encryption operation
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        print_openssl_errors();
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    
    // Set IV length (GCM can use IVs of any length)
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, NULL) != 1) {
        print_openssl_errors();
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    
    // Initialize key and IV
    if (EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        print_openssl_errors();
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    
    // Provide any AAD data
    if (aad && aad_len > 0) {
        if (EVP_EncryptUpdate(ctx, NULL, &len, aad, aad_len) != 1) {
            print_openssl_errors();
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
    }
    
    // Encrypt plaintext
    if (EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len) != 1) {
        print_openssl_errors();
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    ciphertext_len = len;
    
    // Finalize encryption
    if (EVP_EncryptFinal_ex(ctx, ciphertext + len, &len) != 1) {
        print_openssl_errors();
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    ciphertext_len += len;
    
    // Get the tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_SIZE, tag) != 1) {
        print_openssl_errors();
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    
    // Clean up
    EVP_CIPHER_CTX_free(ctx);
    
    return ciphertext_len;
}

// Decrypt data using AES-256-GCM
int decrypt_data(const unsigned char *ciphertext, size_t ciphertext_len,
                const unsigned char *aad, size_t aad_len,
                const unsigned char *tag, const unsigned char *key,
                const unsigned char *iv, unsigned char *plaintext) {
    
    EVP_CIPHER_CTX *ctx;
    int len;
    int plaintext_len;
    int ret;
    
    // Create and initialize the context
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        print_openssl_errors();
        return -1;
    }
    
    // Initialize the decryption operation
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        print_openssl_errors();
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    
    // Set IV length
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, NULL) != 1) {
        print_openssl_errors();
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    
    // Initialize key and IV
    if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        print_openssl_errors();
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    
    // Provide any AAD data
    if (aad && aad_len > 0) {
        if (EVP_DecryptUpdate(ctx, NULL, &len, aad, aad_len) != 1) {
            print_openssl_errors();
            EVP_CIPHER_CTX_free(ctx);
            return -1;
        }
    }
    
    // Decrypt ciphertext
    if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len) != 1) {
        print_openssl_errors();
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    plaintext_len = len;
    
    // Set expected tag value
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_SIZE, (void *)tag) != 1) {
        print_openssl_errors();
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    
    // Finalize decryption: verify tag
    ret = EVP_DecryptFinal_ex(ctx, plaintext + len, &len);
    
    // Clean up
    EVP_CIPHER_CTX_free(ctx);
    
    if (ret > 0) {
        // Tag verified, decryption successful
        plaintext_len += len;
        return plaintext_len;
    } else {
        // Tag verification failed - data corruption or tampering!
        print_openssl_errors();
        return -1;
    }
}

// Server function
void *server_function(void *arg) {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    // Bind socket to port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Server started. Waiting for connections...\n");
    
    // Accept a connection
    if ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Client connected\n");
    
    // Receive and decrypt data
    encrypted_message enc_msg;
    unsigned char plaintext[BUFFER_SIZE];
    
    while (1) {
        // Receive encrypted message
        ssize_t bytes_received = recv(client_fd, &enc_msg, sizeof(enc_msg), 0);
        
        if (bytes_received <= 0) {
            break;  // Connection closed or error
        }
        
        // Decrypt the received message
        int plaintext_len = decrypt_data(enc_msg.ciphertext, enc_msg.ciphertext_len,
                                        enc_msg.aad, AAD_SIZE,
                                        enc_msg.tag, key, enc_msg.iv,
                                        plaintext);
        
        if (plaintext_len < 0) {
            fprintf(stderr, "Decryption failed - message may be corrupted or tampered with\n");
            continue;
        }
        
        // Null-terminate and print plaintext
        plaintext[plaintext_len] = '\0';
        printf("Received decrypted message: %s\n", plaintext);
        
        // Check for exit command
        if (strcmp((char *)plaintext, "exit") == 0) {
            printf("Exit command received. Closing connection.\n");
            break;
        }
        
        // Prepare response
        unsigned char response[BUFFER_SIZE];
        sprintf((char *)response, "Echo: %s", plaintext);
        
        // Generate new IV for response (IV should never be reused with same key)
        if (RAND_bytes(enc_msg.iv, IV_SIZE) != 1) {
            print_openssl_errors();
            break;
        }
        
        // Generate random AAD
        if (RAND_bytes(enc_msg.aad, AAD_SIZE) != 1) {
            print_openssl_errors();
            break;
        }
        
        // Encrypt response
        enc_msg.ciphertext_len = encrypt_data(response, strlen((char *)response),
                                            enc_msg.aad, AAD_SIZE,
                                            key, enc_msg.iv,
                                            enc_msg.ciphertext, enc_msg.tag);
        
        if (enc_msg.ciphertext_len < 0) {
            fprintf(stderr, "Encryption failed\n");
            break;
        }
        
        // Send encrypted response
        send(client_fd, &enc_msg, sizeof(enc_msg), 0);
    }
    
    // Close sockets
    close(client_fd);
    close(server_fd);
    
    printf("Server shutting down\n");
    return NULL;
}

// Client function
void *client_function(void *arg) {
    // Give the server time to start
    sleep(1);
    
    int sock = 0;
    struct sockaddr_in serv_addr;
    
    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Set up server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    // Convert IPv4 address from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address / Address not supported");
        exit(EXIT_FAILURE);
    }
    
    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Connected to server\n");
    
    // Prepare message to send
    const char *messages[] = {
        "Hello from encrypted client!",
        "This message is encrypted with AES-256-GCM",
        "Authenticated encryption provides both confidentiality and integrity",
        "exit"
    };
    
    encrypted_message enc_msg;
    unsigned char plaintext[BUFFER_SIZE];
    
    for (int i = 0; i < 4; i++) {
        // Generate random IV for each message
        if (RAND_bytes(enc_msg.iv, IV_SIZE) != 1) {
            print_openssl_errors();
            break;
        }
        
        // Generate random AAD
        if (RAND_bytes(enc_msg.aad, AAD_SIZE) != 1) {
            print_openssl_errors();
            break;
        }
        
        // Encrypt message
        enc_msg.ciphertext_len = encrypt_data((unsigned char *)messages[i], strlen(messages[i]),
                                            enc_msg.aad, AAD_SIZE,
                                            key, enc_msg.iv,
                                            enc_msg.ciphertext, enc_msg.tag);
        
        if (enc_msg.ciphertext_len < 0) {
            fprintf(stderr, "Encryption failed\n");
            break;
        }
        
        printf("Sending encrypted message: %s\n", messages[i]);
        
        // Send encrypted message
        send(sock, &enc_msg, sizeof(enc_msg), 0);
        
        // Don't wait for response on exit message
        if (i == 3) break;
        
        // Receive encrypted response
        ssize_t bytes_received = recv(sock, &enc_msg, sizeof(enc_msg), 0);
        
        if (bytes_received <= 0) {
            fprintf(stderr, "Server closed connection or error occurred\n");
            break;
        }
        
        // Decrypt response
        int plaintext_len = decrypt_data(enc_msg.ciphertext, enc_msg.ciphertext_len,
                                        enc_msg.aad, AAD_SIZE,
                                        enc_msg.tag, key, enc_msg.iv,
                                        plaintext);
        
        if (plaintext_len < 0) {
            fprintf(stderr, "Decryption failed - response may be corrupted or tampered with\n");
            continue;
        }
        
        // Null-terminate and print plaintext
        plaintext[plaintext_len] = '\0';
        printf("Received decrypted response: %s\n", plaintext);
        
        // Delay between messages
        sleep(1);
    }
    
    // Close socket
    close(sock);
    
    printf("Client shutting down\n");
    return NULL;
}

int main() {
    pthread_t server_thread, client_thread;
    
    // Initialize OpenSSL
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
    
    // Generate encryption key
    generate_key();
    
    // Start server thread
    if (pthread_create(&server_thread, NULL, server_function, NULL) != 0) {
        perror("Failed to create server thread");
        exit(EXIT_FAILURE);
    }
    
    // Start client thread
    if (pthread_create(&client_thread, NULL, client_function, NULL) != 0) {
        perror("Failed to create client thread");
        exit(EXIT_FAILURE);
    }
    
    // Wait for threads to finish
    pthread_join(client_thread, NULL);
    pthread_join(server_thread, NULL);
    
    // Clean up OpenSSL
    EVP_cleanup();
    ERR_free_strings();
    
    return 0;
}