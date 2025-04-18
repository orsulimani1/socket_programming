/**
 * @file sensor_monitoring.c
 * @brief IoT sensor monitoring system using UDP
 * 
 * This example demonstrates a sensor monitoring system for industrial environments.
 * It uses UDP for efficient data collection from multiple sensors.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include "socket_utils.h"
#include "error_handling.h"
#include "config.h"

#define SENSOR_PORT 8888
#define MAX_SENSORS 100
#define MAX_BUFFER_SIZE 1024
#define TEMP_THRESHOLD 85.0  // Temperature threshold in Celsius
#define LOG_FILE "sensor_data.log"

// Flag for graceful shutdown
static volatile int keep_running = 1;

// Structure for sensor data packet
typedef struct {
    uint32_t sensor_id;
    float temperature;
    float pressure;
    float humidity;
    uint32_t timestamp;
} sensor_data_packet;

// Structure for sensor information
typedef struct {
    uint32_t sensor_id;
    char ip_address[INET_ADDRSTRLEN];
    time_t last_update;
    sensor_data_packet last_reading;
} sensor_info;

// Global sensor database
sensor_info sensors[MAX_SENSORS];
int sensor_count = 0;
pthread_mutex_t sensor_mutex = PTHREAD_MUTEX_INITIALIZER;

// Signal handler for graceful shutdown
void handle_signal(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    keep_running = 0;
}

// Function to log sensor data to file
void log_sensor_data(const sensor_data_packet *data, const char *ip_addr) {
    FILE *log_file = fopen(LOG_FILE, "a");
    if (!log_file) {
        perror("Failed to open log file");
        return;
    }
    
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    fprintf(log_file, "%s | Sensor ID: %u | IP: %s | Temp: %.1f°C | Pressure: %.1f kPa | Humidity: %.1f%%\n",
            time_str, data->sensor_id, ip_addr, data->temperature, data->pressure, data->humidity);
    
    fclose(log_file);
}

// Function to send alert on critical condition
void send_alert(const char *message, const sensor_data_packet *data) {
    printf("\033[1;31mALERT: %s\033[0m\n", message);
    printf("Sensor ID: %u | Temperature: %.1f°C | Pressure: %.1f kPa | Humidity: %.1f%%\n",
        data->sensor_id, data->temperature, data->pressure, data->humidity);
    
    // In a real system, this would send alerts via email, SMS, etc.
}

// Function to update or add sensor to database
void update_sensor_database(const sensor_data_packet *data, const char *ip_addr) {
    pthread_mutex_lock(&sensor_mutex);
    
    // Look for existing sensor
    int found = 0;
    for (int i = 0; i < sensor_count; i++) {
        if (sensors[i].sensor_id == data->sensor_id) {
            // Update existing sensor
            sensors[i].last_update = time(NULL);
            memcpy(&sensors[i].last_reading, data, sizeof(sensor_data_packet));
            found = 1;
            break;
        }
    }
    
    // Add new sensor if not found
    if (!found && sensor_count < MAX_SENSORS) {
        sensors[sensor_count].sensor_id = data->sensor_id;
        strncpy(sensors[sensor_count].ip_address, ip_addr, INET_ADDRSTRLEN);
        sensors[sensor_count].last_update = time(NULL);
        memcpy(&sensors[sensor_count].last_reading, data, sizeof(sensor_data_packet));
        sensor_count++;
    }
    
    pthread_mutex_unlock(&sensor_mutex);
}

// Function to check for inactive sensors
void *check_inactive_sensors(void *arg) {
    while (keep_running) {
        sleep(60);  // Check every minute
        
        time_t current_time = time(NULL);
        pthread_mutex_lock(&sensor_mutex);
        
        for (int i = 0; i < sensor_count; i++) {
            // If sensor hasn't updated in 5 minutes, log warning
            if (difftime(current_time, sensors[i].last_update) > 300) {
                printf("\033[1;33mWARNING: Sensor %u (IP: %s) hasn't reported in %ld seconds\033[0m\n",
                    sensors[i].sensor_id, sensors[i].ip_address, 
                    (long)difftime(current_time, sensors[i].last_update));
            }
        }
        
        pthread_mutex_unlock(&sensor_mutex);
    }
    
    return NULL;
}

// Function to display sensor statistics
void display_sensor_stats() {
    pthread_mutex_lock(&sensor_mutex);
    
    printf("\n--- Sensor Statistics ---\n");
    printf("Active sensors: %d\n", sensor_count);
    
    // Calculate average temperature across all sensors
    float avg_temp = 0.0;
    float avg_pressure = 0.0;
    float avg_humidity = 0.0;
    
    for (int i = 0; i < sensor_count; i++) {
        avg_temp += sensors[i].last_reading.temperature;
        avg_pressure += sensors[i].last_reading.pressure;
        avg_humidity += sensors[i].last_reading.humidity;
    }
    
    if (sensor_count > 0) {
        avg_temp /= sensor_count;
        avg_pressure /= sensor_count;
        avg_humidity /= sensor_count;
        
        printf("Average temperature: %.1f°C\n", avg_temp);
        printf("Average pressure: %.1f kPa\n", avg_pressure);
        printf("Average humidity: %.1f%%\n", avg_humidity);
    }
    
    pthread_mutex_unlock(&sensor_mutex);
}

int main() {
    // Set up signal handling for graceful shutdown
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // Initialize logging
    printf("Starting sensor monitoring system...\n");
    
    // Create UDP socket
    int sockfd = create_udp_socket(0, 0); // No broadcast, blocking mode
    if (sockfd < 0) {
        FATAL_ERRNO("Failed to create socket");
    }
    
    // Bind socket to specific port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SENSOR_PORT);
    
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        FATAL_ERRNO("Failed to bind socket to port %d", SENSOR_PORT);
    }
    
    printf("Sensor monitoring system started on port %d\n", SENSOR_PORT);
    
    // Create thread to check for inactive sensors
    pthread_t inactive_thread;
    pthread_create(&inactive_thread, NULL, check_inactive_sensors, NULL);
    
    // Counter for statistics display
    int packet_counter = 0;
    
    // Main loop to receive sensor data
    while (keep_running) {
        // Buffer for incoming data
        char buffer[MAX_BUFFER_SIZE];
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        // Receive data from sensors
        ssize_t bytes_received = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                        (struct sockaddr *)&client_addr, &addr_len);
        
        if (bytes_received < 0) {
            if (errno == EINTR) {
                // Interrupted by signal, check if we should keep running
                continue;
            }
            LOG_ERRNO("Error receiving sensor data");
            continue;
        }
        
        // Get sensor IP address
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        
        // Process the received data
        if (bytes_received == sizeof(sensor_data_packet)) {
            // Parse sensor data packet
            sensor_data_packet *data = (sensor_data_packet *)buffer;
            
            printf("Received data from sensor %u at %s - Temp: %.1f°C, Pressure: %.1f kPa, Humidity: %.1f%%\n",
                data->sensor_id, client_ip, data->temperature, data->pressure, data->humidity);
            
            // Check for critical conditions
            if (data->temperature > TEMP_THRESHOLD) {
                send_alert("High temperature detected", data);
            }
            
            // Log data to database and file
            update_sensor_database(data, client_ip);
            log_sensor_data(data, client_ip);
            
            // Display stats every 10 packets
            if (++packet_counter % 10 == 0) {
                display_sensor_stats();
            }
        } else {
            printf("Received invalid packet size from %s (expected %zu, got %zd bytes)\n",
                client_ip, sizeof(sensor_data_packet), bytes_received);
        }
    }
    
    // Clean up
    printf("Shutting down sensor monitoring system...\n");
    pthread_cancel(inactive_thread);
    pthread_join(inactive_thread, NULL);
    pthread_mutex_destroy(&sensor_mutex);
    close(sockfd);
    
    printf("Sensor monitoring system stopped\n");
    
    return 0;
}