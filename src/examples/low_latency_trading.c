/**
 * @file low_latency_trading.c
 * @brief Low-latency trading system using raw sockets
 * 
 * This example demonstrates a low-latency financial trading system
 * using raw sockets and kernel bypass techniques to minimize latency.
 * 
 * Note: This example requires root privileges to run as it uses raw sockets.
 * Compile with: gcc -o low_latency_trading low_latency_trading.c -lrt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include "socket_utils.h"
#include "error_handling.h"
#include "config.h"

// Check if running on Linux
#ifndef __linux__
#error "This example requires Linux-specific features"
#endif

// Configuration
#define INTERFACE_NAME "eth0"   // Network interface to use
#define MARKET_IP "239.0.0.1"   // Multicast IP for market data
#define MARKET_PORT 30001       // Market data port
#define EXCHANGE_IP "10.0.0.10" // Exchange IP address
#define EXCHANGE_PORT 30002     // Exchange order port
#define PACKET_BUFFER_SIZE 2048 // Size of packet buffer
#define MAX_SYMBOLS 100         // Maximum number of symbols to track
#define RING_SIZE 2048          // Size of ring buffer (must be power of 2)
#define NSEC_PER_SEC 1000000000L

// Flag for graceful shutdown
static volatile int keep_running = 1;

// Symbol price data
typedef struct {
    char symbol[16];         // Symbol name (e.g., "AAPL")
    double last_price;       // Last trade price
    double bid;              // Current bid price
    double ask;              // Current ask price
    uint64_t timestamp_ns;   // Timestamp in nanoseconds
    uint32_t volume;         // Trading volume
} market_data_t;

// Trading order
typedef struct {
    char symbol[16];         // Symbol to trade
    char side;               // 'B' for buy, 'S' for sell
    double price;            // Limit price (0 for market order)
    uint32_t quantity;       // Number of shares
    uint32_t order_id;       // Order ID
} trading_order_t;

// Market data store
market_data_t market_data[MAX_SYMBOLS];
int num_symbols = 0;

// Performance metrics
typedef struct {
    uint64_t packets_received;
    uint64_t packets_sent;
    uint64_t orders_sent;
    uint64_t min_latency_ns;
    uint64_t max_latency_ns;
    uint64_t total_latency_ns;
    uint64_t latency_samples;
} metrics_t;

metrics_t metrics = {0};

// Signal handler for graceful shutdown
void handle_signal(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    keep_running = 0;
}

// Get current time in nanoseconds
uint64_t get_timestamp_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ((uint64_t)ts.tv_sec * NSEC_PER_SEC) + ts.tv_nsec;
}

// Calculate latency in nanoseconds
void track_latency(uint64_t start_ns, uint64_t end_ns) {
    if (start_ns >= end_ns) {
        printf("Warning: Invalid timestamps for latency calculation\n");
        return;
    }
    
    uint64_t latency = end_ns - start_ns;
    
    // Update metrics
    if (metrics.latency_samples == 0 || latency < metrics.min_latency_ns) {
        metrics.min_latency_ns = latency;
    }
    if (latency > metrics.max_latency_ns) {
        metrics.max_latency_ns = latency;
    }
    
    metrics.total_latency_ns += latency;
    metrics.latency_samples++;
    
    // Log significant latency events
    if (latency > 1000000) { // >1ms is considered high for HFT
        printf("High latency detected: %.3f ms\n", latency / 1000000.0);
    }
}

// Update market data
void update_market_data(const char *symbol, double price, double bid, double ask, uint32_t volume) {
    uint64_t now = get_timestamp_ns();
    
    // Look for existing symbol
    for (int i = 0; i < num_symbols; i++) {
        if (strcmp(market_data[i].symbol, symbol) == 0) {
            market_data[i].last_price = price;
            market_data[i].bid = bid;
            market_data[i].ask = ask;
            market_data[i].volume = volume;
            market_data[i].timestamp_ns = now;
            return;
        }
    }
    
    // Add new symbol if space available
    if (num_symbols < MAX_SYMBOLS) {
        strncpy(market_data[num_symbols].symbol, symbol, sizeof(market_data[0].symbol) - 1);
        market_data[num_symbols].symbol[sizeof(market_data[0].symbol) - 1] = '\0';
        market_data[num_symbols].last_price = price;
        market_data[num_symbols].bid = bid;
        market_data[num_symbols].ask = ask;
        market_data[num_symbols].volume = volume;
        market_data[num_symbols].timestamp_ns = now;
        num_symbols++;
    }
}

// Trading strategy (simple example)
int should_execute_trade(const market_data_t *data) {
    static uint64_t last_trade_time = 0;
    uint64_t now = get_timestamp_ns();
    
    // Limit trading frequency (no more than one trade per 100ms)
    if (now - last_trade_time < 100000000) {
        return 0;
    }
    
    // Simple momentum strategy (for demonstration)
    // In a real system, this would be a sophisticated algorithm
    if (data->bid > data->last_price && data->volume > 1000) {
        last_trade_time = now;
        return 1; // Buy signal
    }
    
    return 0;
}

// Parse market data packet
void parse_market_data(const char *packet, size_t length) {
    if (length < 32) { // Minimum valid packet size
        return;
    }
    
    // In a real system, this would parse the exchange's binary protocol
    // This is a simplified example assuming a text-based format
    char symbol[16];
    double price, bid, ask;
    uint32_t volume;
    
    // Parse the packet (simplified)
    if (sscanf(packet, "%15s %lf %lf %lf %u", symbol, &price, &bid, &ask, &volume) == 5) {
        update_market_data(symbol, price, bid, ask, volume);
    }
}

// Send trading order
int send_trading_order(int sockfd, const struct sockaddr_in *addr, const market_data_t *data) {
    // Create order
    trading_order_t order;
    strncpy(order.symbol, data->symbol, sizeof(order.symbol));
    order.side = 'B'; // Buy
    order.price = data->ask; // Buy at ask price
    order.quantity = 100; // Fixed quantity for demonstration
    
    // Generate order ID
    static uint32_t next_order_id = 1;
    order.order_id = next_order_id++;
    
    // Track time for latency measurement
    uint64_t start_time = get_timestamp_ns();
    
    // Send order
    ssize_t bytes_sent = sendto(sockfd, &order, sizeof(order), 0,
                            (const struct sockaddr *)addr, sizeof(*addr));
    
    // Calculate latency
    uint64_t end_time = get_timestamp_ns();
    track_latency(start_time, end_time);
    
    if (bytes_sent < 0) {
        LOG_ERRNO("Failed to send trading order");
        return -1;
    }
    
    // Update metrics
    metrics.orders_sent++;
    
    printf("Sent %s order for %s: %u shares at $%.2f (Order ID: %u, Latency: %.2f µs)\n",
        order.side == 'B' ? "BUY" : "SELL",
        order.symbol,
        order.quantity,
        order.price,
        order.order_id,
        (end_time - start_time) / 1000.0);
    
    return 0;
}

// Configure network interface for low latency
int configure_interface_low_latency(const char *interface_name) {
    // Disable interrupt coalescing
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/napi_defer_hard_irqs", interface_name);
    
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
        write(fd, "0", 1);
        close(fd);
    }
    
    // Set RX/TX ring parameters
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }
    
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    
    // Get interface index
    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        perror("SIOCGIFINDEX");
        close(sock);
        return -1;
    }
    
    close(sock);
    return 0;
}

// Setup raw socket for market data
int setup_market_data_socket(const char *interface_name) {
    int sockfd;
    struct sockaddr_ll addr;
    struct ifreq ifr;
    
    // Create raw socket
    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }
    
    // Get interface index
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
        perror("SIOCGIFINDEX");
        close(sockfd);
        return -1;
    }
    
    // Bind to interface
    memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_ALL);
    addr.sll_ifindex = ifr.ifr_ifindex;
    
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        return -1;
    }
    
    // Set socket to non-blocking mode
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    
    // Setup packet ring buffer for zero-copy operation
    struct tpacket_req req;
    memset(&req, 0, sizeof(req));
    req.tp_block_size = getpagesize() * 8;
    req.tp_block_nr = 64;
    req.tp_frame_size = TPACKET_ALIGNMENT << 7;
    req.tp_frame_nr = req.tp_block_size * req.tp_block_nr / req.tp_frame_size;
    
    if (setsockopt(sockfd, SOL_PACKET, PACKET_RX_RING, &req, sizeof(req)) < 0) {
        perror("setsockopt(PACKET_RX_RING)");
        // Fall back to standard mode if ring buffer not supported
        printf("Falling back to standard socket mode\n");
    } else {
        printf("Using zero-copy packet ring buffer\n");
    }
    
    // Set high priority for socket
    int priority = 7;  // Highest non-root priority
    if (setsockopt(sockfd, SOL_SOCKET, SO_PRIORITY, &priority, sizeof(priority)) < 0) {
        perror("setsockopt(SO_PRIORITY)");
    }
    
    return sockfd;
}

// Setup UDP socket for sending orders
int setup_order_socket(const char *exchange_ip, int exchange_port) {
    int sockfd;
    struct sockaddr_in addr;
    
    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }
    
    // Set socket options for low latency
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt(SO_REUSEADDR)");
    }
    
    // Disable Nagle's algorithm for UDP
    if (setsockopt(sockfd, IPPROTO_IP, IP_TOS, &optval, sizeof(optval)) < 0) {
        perror("setsockopt(IP_TOS)");
    }
    
    // Bind to local address (optional for sending)
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0;  // Let OS choose port
    
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

// Display performance metrics
void display_metrics() {
    printf("\n=== Performance Metrics ===\n");
    printf("Packets received: %lu\n", metrics.packets_received);
    printf("Orders sent: %lu\n", metrics.orders_sent);
    
    if (metrics.latency_samples > 0) {
        printf("Minimum latency: %.3f µs\n", metrics.min_latency_ns / 1000.0);
        printf("Maximum latency: %.3f µs\n", metrics.max_latency_ns / 1000.0);
        printf("Average latency: %.3f µs\n", 
            (metrics.total_latency_ns / metrics.latency_samples) / 1000.0);
    }
}

int main() {
    // Set up signal handling for graceful shutdown
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // Lock memory to prevent paging
    if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0) {
        perror("mlockall");
        printf("Warning: Could not lock memory, may experience latency spikes\n");
    }
    
    // Configure CPU affinity to pin process to a specific core
    // This would typically be done with sched_setaffinity()
    
    // Configure interface for low latency
    if (configure_interface_low_latency(INTERFACE_NAME) < 0) {
        fprintf(stderr, "Warning: Could not configure interface for low latency\n");
    }
    
    // Set up raw socket for market data
    int market_sock = setup_market_data_socket(INTERFACE_NAME);
    if (market_sock < 0) {
        fprintf(stderr, "Failed to set up market data socket\n");
        return 1;
    }
    
    // Set up UDP socket for sending orders
    int order_sock = setup_order_socket(EXCHANGE_IP, EXCHANGE_PORT);
    if (order_sock < 0) {
        fprintf(stderr, "Failed to set up order socket\n");
        close(market_sock);
        return 1;
    }
    
    // Prepare address for sending orders
    struct sockaddr_in exchange_addr;
    memset(&exchange_addr, 0, sizeof(exchange_addr));
    exchange_addr.sin_family = AF_INET;
    inet_pton(AF_INET, EXCHANGE_IP, &exchange_addr.sin_addr);
    exchange_addr.sin_port = htons(EXCHANGE_PORT);
    
    printf("Low-latency trading system initialized\n");
    printf("Monitoring market data on interface %s\n", INTERFACE_NAME);
    printf("Sending orders to %s:%d\n", EXCHANGE_IP, EXCHANGE_PORT);
    printf("Press Ctrl+C to exit\n\n");
    
    // Buffer for receiving market data
    char buffer[PACKET_BUFFER_SIZE];
    
    // Main trading loop
    while (keep_running) {
        // Receive market data packets
        ssize_t bytes_received = recv(market_sock, buffer, sizeof(buffer), 0);
        
        if (bytes_received > 0) {
            // Skip Ethernet, IP and UDP headers
            // In a real system, we would parse these headers
            const size_t header_size = 14 + 20 + 8;  // Ethernet + IP + UDP
            
            if (bytes_received > header_size) {
                // Update metrics
                metrics.packets_received++;
                
                // Process market data
                parse_market_data(buffer + header_size, bytes_received - header_size);
                
                // Check trading signals
                for (int i = 0; i < num_symbols; i++) {
                    if (should_execute_trade(&market_data[i])) {
                        // Execute trade
                        send_trading_order(order_sock, &exchange_addr, &market_data[i]);
                    }
                }
            }
        } else if (bytes_received < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            // Handle error
            perror("recv");
            break;
        }
        
        // Small delay to prevent CPU hogging in this example
        // In a real HFT system, we would use busy-waiting or event-driven approach
        usleep(100);
    }
    
    // Display performance metrics
    display_metrics();
    
    // Clean up
    close(market_sock);
    close(order_sock);
    
    printf("Low-latency trading system shut down\n");
    
    return 0;
}