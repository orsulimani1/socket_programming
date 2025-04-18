/**
 * @file socket_utils.h
 * @brief Common utility functions for socket programming
 * 
 * This header provides reusable functions for common socket operations
 * such as creating sockets, setting options, and handling common error cases.
 */

#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

/**
 * @brief Creates a TCP socket and configures common options
 * 
 * @param reuseaddr Set to 1 to enable SO_REUSEADDR option
 * @param nonblocking Set to 1 to make the socket non-blocking
 * @return Socket file descriptor or -1 on error
 */
static inline int create_tcp_socket(int reuseaddr, int nonblocking) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        return -1;
    }
    
    // Set SO_REUSEADDR if requested
    if (reuseaddr) {
        int opt = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("setsockopt(SO_REUSEADDR) failed");
            close(sockfd);
            return -1;
        }
    }
    
    // Set non-blocking mode if requested
    if (nonblocking) {
        int flags = fcntl(sockfd, F_GETFL, 0);
        if (flags < 0 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
            perror("failed to set non-blocking mode");
            close(sockfd);
            return -1;
        }
    }
    
    return sockfd;
}

/**
 * @brief Creates a UDP socket and configures common options
 * 
 * @param broadcast Set to 1 to enable SO_BROADCAST option
 * @param nonblocking Set to 1 to make the socket non-blocking
 * @return Socket file descriptor or -1 on error
 */
static inline int create_udp_socket(int broadcast, int nonblocking) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        return -1;
    }
    
    // Set SO_BROADCAST if requested
    if (broadcast) {
        int opt = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0) {
            perror("setsockopt(SO_BROADCAST) failed");
            close(sockfd);
            return -1;
        }
    }
    
    // Set non-blocking mode if requested
    if (nonblocking) {
        int flags = fcntl(sockfd, F_GETFL, 0);
        if (flags < 0 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
            perror("failed to set non-blocking mode");
            close(sockfd);
            return -1;
        }
    }
    
    return sockfd;
}

/**
 * @brief Creates a Unix Domain Socket
 * 
 * @param stream Set to 1 for SOCK_STREAM, 0 for SOCK_DGRAM
 * @return Socket file descriptor or -1 on error
 */
static inline int create_unix_socket(int stream) {
    int type = stream ? SOCK_STREAM : SOCK_DGRAM;
    int sockfd = socket(AF_UNIX, type, 0);
    if (sockfd < 0) {
        perror("unix socket creation failed");
    }
    return sockfd;
}

/**
 * @brief Set socket receive timeout
 * 
 * @param sockfd Socket file descriptor
 * @param seconds Timeout seconds
 * @param microseconds Timeout microseconds
 * @return 0 on success, -1 on failure
 */
static inline int set_socket_timeout(int sockfd, int seconds, int microseconds) {
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = microseconds;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt(SO_RCVTIMEO) failed");
        return -1;
    }
    
    return 0;
}

/**
 * @brief Disable Nagle's algorithm for lower latency
 * 
 * @param sockfd Socket file descriptor
 * @return 0 on success, -1 on failure
 */
static inline int disable_nagle(int sockfd) {
    int flag = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
        perror("setsockopt(TCP_NODELAY) failed");
        return -1;
    }
    return 0;
}

/**
 * @brief Set keep-alive options for a TCP socket
 * 
 * @param sockfd Socket file descriptor
 * @param idle_time Time in seconds before sending first keepalive (default: 7200)
 * @param interval Interval in seconds between keepalives (default: 75)
 * @param max_probes Max number of keepalives before giving up (default: 9)
 * @return 0 on success, -1 on failure
 */
static inline int set_keepalive(int sockfd, int idle_time, int interval, int max_probes) {
    int flag = 1;
    
    // Enable SO_KEEPALIVE
    if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) < 0) {
        perror("setsockopt(SO_KEEPALIVE) failed");
        return -1;
    }
    
#ifdef TCP_KEEPIDLE  // Linux-specific
    // Set time before sending keepalive probes
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &idle_time, sizeof(idle_time)) < 0) {
        perror("setsockopt(TCP_KEEPIDLE) failed");
        return -1;
    }
    
    // Set interval between keepalive probes
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) < 0) {
        perror("setsockopt(TCP_KEEPINTVL) failed");
        return -1;
    }
    
    // Set number of keepalive probes before giving up
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &max_probes, sizeof(max_probes)) < 0) {
        perror("setsockopt(TCP_KEEPCNT) failed");
        return -1;
    }
#endif

    return 0;
}

/**
 * @brief Convert IP address to string
 * 
 * @param addr Pointer to sockaddr structure
 * @param buf Buffer to store IP string
 * @param size Size of buffer
 * @return Pointer to buffer or NULL on error
 */
static inline char* get_ip_str(const struct sockaddr *addr, char *buf, size_t size) {
    if (addr->sa_family == AF_INET) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)addr;
        return inet_ntop(AF_INET, &ipv4->sin_addr, buf, size);
    } else if (addr->sa_family == AF_INET6) {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)addr;
        return inet_ntop(AF_INET6, &ipv6->sin6_addr, buf, size);
    } else {
        return NULL;
    }
}

/**
 * @brief Print socket address information
 * 
 * @param addr Pointer to sockaddr structure
 * @param is_server 1 if server address, 0 if client
 */
static inline void print_socket_info(const struct sockaddr *addr, int is_server) {
    char ip_str[INET6_ADDRSTRLEN];
    int port;
    
    if (addr->sa_family == AF_INET) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)addr;
        inet_ntop(AF_INET, &ipv4->sin_addr, ip_str, sizeof(ip_str));
        port = ntohs(ipv4->sin_port);
    } else if (addr->sa_family == AF_INET6) {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)addr;
        inet_ntop(AF_INET6, &ipv6->sin6_addr, ip_str, sizeof(ip_str));
        port = ntohs(ipv6->sin6_port);
    } else {
        printf("Unknown address family\n");
        return;
    }
    
    printf("%s address: %s, port: %d\n", 
        is_server ? "Server" : "Client", ip_str, port);
}

/**
 * @brief Connect to server with timeout
 * 
 * @param sockfd Socket file descriptor
 * @param addr Server address
 * @param addrlen Address length
 * @param timeout_sec Connection timeout in seconds
 * @return 0 on success, -1 on failure
 */
static inline int connect_with_timeout(int sockfd, const struct sockaddr *addr, 
                                    socklen_t addrlen, int timeout_sec) {
    // Save current socket flags
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        perror("fcntl F_GETFL failed");
        return -1;
    }
    
    // Set socket to non-blocking mode for timeout
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl F_SETFL O_NONBLOCK failed");
        return -1;
    }
    
    // Attempt to connect
    int ret = connect(sockfd, addr, addrlen);
    if (ret < 0 && errno != EINPROGRESS) {
        perror("connect failed immediately");
        fcntl(sockfd, F_SETFL, flags);  // Restore flags
        return -1;
    }
    
    if (ret < 0) {  // EINPROGRESS
        // Use select to wait for connection completion
        fd_set write_fds;
        struct timeval tv;
        
        FD_ZERO(&write_fds);
        FD_SET(sockfd, &write_fds);
        tv.tv_sec = timeout_sec;
        tv.tv_usec = 0;
        
        ret = select(sockfd + 1, NULL, &write_fds, NULL, &tv);
        
        if (ret == 0) {
            // Timeout
            errno = ETIMEDOUT;
            fcntl(sockfd, F_SETFL, flags);  // Restore flags
            return -1;
        } else if (ret < 0) {
            // Select error
            perror("select failed");
            fcntl(sockfd, F_SETFL, flags);  // Restore flags
            return -1;
        }
        
        // Check if connection succeeded
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
            perror("getsockopt failed");
            fcntl(sockfd, F_SETFL, flags);  // Restore flags
            return -1;
        }
        
        if (error) {
            // Connection failed
            errno = error;
            fcntl(sockfd, F_SETFL, flags);  // Restore flags
            return -1;
        }
    }
    
    // Restore socket flags (blocking mode)
    if (fcntl(sockfd, F_SETFL, flags) < 0) {
        perror("fcntl failed to restore flags");
        return -1;
    }
    
    return 0;  // Connection successful
}

/**
 * @brief Safely read exactly n bytes from a socket
 * 
 * Continues reading until all requested bytes are received, 
 * or until an error or connection close occurs.
 * 
 * @param sockfd Socket file descriptor
 * @param buf Buffer to store data
 * @param n Number of bytes to read
 * @return Number of bytes read, or -1 on error
 */
static inline ssize_t read_n_bytes(int sockfd, void *buf, size_t n) {
    size_t remaining = n;
    size_t bytes_read = 0;
    char *ptr = (char *)buf;
    
    while (remaining > 0) {
        ssize_t res = read(sockfd, ptr + bytes_read, remaining);
        
        if (res < 0) {
            // Error occurred
            if (errno == EINTR) {
                // Interrupted, try again
                continue;
            }
            return -1;
        } else if (res == 0) {
            // EOF, connection closed
            break;
        }
        
        remaining -= res;
        bytes_read += res;
    }
    
    return bytes_read;
}

/**
 * @brief Safely write exactly n bytes to a socket
 * 
 * Continues writing until all requested bytes are sent,
 * or until an error occurs.
 * 
 * @param sockfd Socket file descriptor
 * @param buf Buffer containing data to write
 * @param n Number of bytes to write
 * @return Number of bytes written, or -1 on error
 */
static inline ssize_t write_n_bytes(int sockfd, const void *buf, size_t n) {
    size_t remaining = n;
    size_t bytes_written = 0;
    const char *ptr = (const char *)buf;
    
    while (remaining > 0) {
        ssize_t res = write(sockfd, ptr + bytes_written, remaining);
        
        if (res < 0) {
            // Error occurred
            if (errno == EINTR) {
                // Interrupted, try again
                continue;
            }
            return -1;
        }
        
        remaining -= res;
        bytes_written += res;
    }
    
    return bytes_written;
}

#endif /* SOCKET_UTILS_H */