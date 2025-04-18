/**
 * @file can_automotive.c
 * @brief Real-time automotive system using CAN sockets
 * 
 * This example demonstrates a real-time communication system for
 * automotive control modules using Controller Area Network (CAN) sockets.
 * 
 * Note: This example requires Linux with SocketCAN support.
 * Compile with: gcc -o can_automotive can_automotive.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include "socket_utils.h"
#include "error_handling.h"
#include "config.h"

// CAN interface name
#define CAN_INTERFACE "can0"

// CAN message IDs for different modules
#define ENGINE_CAN_ID     0x100  // Engine control module
#define BRAKE_CAN_ID      0x200  // Brake control module
#define STEERING_CAN_ID   0x300  // Steering control module
#define DASHBOARD_CAN_ID  0x400  // Dashboard module
#define DIAGNOSTIC_CAN_ID 0x700  // Diagnostic messages

// Flag for graceful shutdown
static volatile int keep_running = 1;

// Signal handler for graceful shutdown
void handle_signal(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    keep_running = 0;
}

// Structure for engine data
typedef struct {
    uint16_t rpm;              // Engine RPM
    uint8_t temperature;       // Engine temperature in Celsius
    uint8_t throttle_position; // Throttle position (0-100%)
    uint16_t fuel_level;       // Fuel level in milliliters
    uint8_t engine_status;     // Engine status flags
} engine_data_t;

// Structure for brake data
typedef struct {
    uint8_t brake_position;    // Brake pedal position (0-100%)
    uint8_t brake_pressure;    // Brake system pressure (0-255)
    uint8_t abs_active;        // ABS status (0=inactive, 1=active)
    uint8_t brake_status;      // Brake status flags
} brake_data_t;

// Structure for steering data
typedef struct {
    int16_t steering_angle;    // Steering angle in 1/10 degrees (-1800 to +1800)
    uint8_t steering_speed;    // Steering movement speed
    uint8_t steering_status;   // Steering system status flags
} steering_data_t;

// Function to extract engine data from CAN frame
void process_engine_data(const struct can_frame *frame) {
    if (frame->can_dlc < 6) {
        printf("Error: Engine data frame too short\n");
        return;
    }
    
    engine_data_t data;
    data.rpm = (frame->data[0] << 8) | frame->data[1];
    data.temperature = frame->data[2];
    data.throttle_position = frame->data[3];
    data.fuel_level = (frame->data[4] << 8) | frame->data[5];
    data.engine_status = (frame->can_dlc >= 7) ? frame->data[6] : 0;
    
    printf("Engine: RPM=%u, Temp=%d°C, Throttle=%u%%, Fuel=%u ml, Status=0x%02X\n",
        data.rpm, data.temperature, data.throttle_position, 
        data.fuel_level, data.engine_status);
    
    // In a real system, this data would be processed by the appropriate ECU
}

// Function to extract brake data from CAN frame
void process_brake_data(const struct can_frame *frame) {
    if (frame->can_dlc < 4) {
        printf("Error: Brake data frame too short\n");
        return;
    }
    
    brake_data_t data;
    data.brake_position = frame->data[0];
    data.brake_pressure = frame->data[1];
    data.abs_active = frame->data[2];
    data.brake_status = frame->data[3];
    
    printf("Brake: Position=%u%%, Pressure=%u, ABS=%s, Status=0x%02X\n",
        data.brake_position, data.brake_pressure,
        data.abs_active ? "Active" : "Inactive", data.brake_status);
    
    // In a real system, this data would be processed by the appropriate ECU
}

// Function to extract steering data from CAN frame
void process_steering_data(const struct can_frame *frame) {
    if (frame->can_dlc < 3) {
        printf("Error: Steering data frame too short\n");
        return;
    }
    
    steering_data_t data;
    // Convert from two's complement for steering angle
    int16_t raw_angle = (frame->data[0] << 8) | frame->data[1];
    data.steering_angle = raw_angle;
    data.steering_speed = frame->data[2];
    data.steering_status = (frame->can_dlc >= 4) ? frame->data[3] : 0;
    
    printf("Steering: Angle=%.1f°, Speed=%u, Status=0x%02X\n",
        data.steering_angle / 10.0, data.steering_speed, data.steering_status);
    
    // In a real system, this data would be processed by the appropriate ECU
}

// Check if emergency braking is requested
int is_emergency_braking(const struct can_frame *frame) {
    if (frame->can_dlc < 4) {
        return 0;
    }
    
    brake_data_t data;
    data.brake_position = frame->data[0];
    data.brake_pressure = frame->data[1];
    
    // Consider it emergency braking if brake position > 80% and pressure is high
    return (data.brake_position > 80 && data.brake_pressure > 200);
}

// Send emergency signal to all modules
void send_emergency_signal(int sockfd) {
    struct can_frame frame;
    memset(&frame, 0, sizeof(frame));
    
    // Set up emergency frame
    frame.can_id = DIAGNOSTIC_CAN_ID;
    frame.can_dlc = 2;
    frame.data[0] = 0xFF;  // Emergency code
    frame.data[1] = 0x01;  // Emergency type: Brake
    
    // Send emergency frame
    if (write(sockfd, &frame, sizeof(frame)) < 0) {
        perror("Error sending emergency signal");
    } else {
        printf("Emergency signal sent to all modules\n");
    }
}

// Send dashboard update with current vehicle status
void send_dashboard_update(int sockfd) {
    static uint8_t counter = 0;
    struct can_frame frame;
    memset(&frame, 0, sizeof(frame));
    
    // Set up dashboard update frame
    frame.can_id = DASHBOARD_CAN_ID;
    frame.can_dlc = 8;
    
    // Dummy data for demonstration - in a real system this would be real sensor data
    frame.data[0] = 55;         // Current speed (55 km/h)
    frame.data[1] = 90;         // Engine temperature (90°C)
    frame.data[2] = 75;         // Fuel level (75%)
    frame.data[3] = 0;          // Warning flags
    frame.data[4] = 0;          // Error flags
    frame.data[5] = 1;          // Gear position (1)
    frame.data[6] = 0;          // Reserved
    frame.data[7] = counter++;  // Message counter for detecting missed frames
    
    // Send dashboard update
    if (write(sockfd, &frame, sizeof(frame)) < 0) {
        perror("Error sending dashboard update");
    }
}

// Check if dashboard update should be sent
int should_update_dashboard() {
    // In this example, we update every ~500ms
    static time_t last_update = 0;
    time_t now = time(NULL);
    
    if (now - last_update >= 1) {
        last_update = now;
        return 1;
    }
    
    return 0;
}

// Initialize CAN interface
int initialize_can_interface(const char *interface) {
    int sockfd;
    struct sockaddr_can addr;
    struct ifreq ifr;
    
    // Create CAN raw socket
    sockfd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sockfd < 0) {
        perror("Error creating CAN socket");
        return -1;
    }
    
    // Get interface index
    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
        perror("Error getting interface index");
        close(sockfd);
        return -1;
    }
    
    // Bind socket to CAN interface
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Error binding CAN socket");
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

// Set up filters for specific CAN IDs
int setup_can_filters(int sockfd) {
    struct can_filter filters[4];
    
    // We're interested in these message types
    filters[0].can_id = ENGINE_CAN_ID;
    filters[0].can_mask = CAN_SFF_MASK;
    
    filters[1].can_id = BRAKE_CAN_ID;
    filters[1].can_mask = CAN_SFF_MASK;
    
    filters[2].can_id = STEERING_CAN_ID;
    filters[2].can_mask = CAN_SFF_MASK;
    
    filters[3].can_id = DIAGNOSTIC_CAN_ID;
    filters[3].can_mask = CAN_SFF_MASK;
    
    if (setsockopt(sockfd, SOL_CAN_RAW, CAN_RAW_FILTER, filters, sizeof(filters)) < 0) {
        perror("Error setting CAN filters");
        return -1;
    }
    
    return 0;
}

int main() {
    // Set up signal handling for graceful shutdown
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    printf("Starting automotive CAN communication system\n");
    
    // Initialize CAN interface
    int sockfd = initialize_can_interface(CAN_INTERFACE);
    if (sockfd < 0) {
        fprintf(stderr, "Failed to initialize CAN interface %s\n", CAN_INTERFACE);
        return 1;
    }
    
    // Set up filters for the CAN IDs we're interested in
    if (setup_can_filters(sockfd) < 0) {
        fprintf(stderr, "Failed to set up CAN filters\n");
        close(sockfd);
        return 1;
    }
    
    printf("CAN communication initialized on interface %s\n", CAN_INTERFACE);
    printf("Monitoring for engine, brake, and steering messages\n");
    printf("Press Ctrl+C to exit\n\n");
    
    // Main loop
    struct can_frame frame;
    
    while (keep_running) {
        // Receive CAN frame
        ssize_t nbytes = read(sockfd, &frame, sizeof(frame));
        
        if (nbytes < 0) {
            // Error reading from socket
            if (errno == EINTR) {
                // Interrupted by signal, check if we should continue
                continue;
            }
            
            perror("Error reading from CAN socket");
            break;
        }
        
        if (nbytes < (ssize_t)sizeof(struct can_frame)) {
            fprintf(stderr, "Incomplete CAN frame received\n");
            continue;
        }
        
        // Process based on CAN ID
        switch (frame.can_id) {
            case ENGINE_CAN_ID:
                process_engine_data(&frame);
                break;
                
            case BRAKE_CAN_ID:
                process_brake_data(&frame);
                
                // Check for emergency braking
                if (is_emergency_braking(&frame)) {
                    printf("EMERGENCY BRAKING DETECTED!\n");
                    send_emergency_signal(sockfd);
                }
                break;
                
            case STEERING_CAN_ID:
                process_steering_data(&frame);
                break;
                
            case DIAGNOSTIC_CAN_ID:
                printf("Diagnostic message received: ID=0x%X, Data=[%02X %02X %02X %02X %02X %02X %02X %02X]\n",
                    frame.can_id,
                    frame.data[0], frame.data[1], frame.data[2], frame.data[3],
                    frame.data[4], frame.data[5], frame.data[6], frame.data[7]);
                break;
                
            default:
                // Unknown/unhandled CAN ID
                printf("Received message with unhandled CAN ID: 0x%X\n", frame.can_id);
                break;
        }
        
        // Send status update to dashboard if needed
        if (should_update_dashboard()) {
            send_dashboard_update(sockfd);
        }
    }
    
    // Clean up
    close(sockfd);
    printf("CAN communication system shut down\n");
    
    return 0;
}