/**
 * @file config.h
 * @brief Configuration parameters for socket programming examples
 * 
 * This header provides centralized configuration parameters for all
 * socket programming examples. It can be adjusted based on the target
 * embedded platform's capabilities and constraints.
 */

 #ifndef CONFIG_H
 #define CONFIG_H
 
 /**
  * @brief Network Configuration
  */
 #define DEFAULT_SERVER_PORT 8080   /**< Default port number for server examples */
 #define DEFAULT_SERVER_IP "127.0.0.1" /**< Default server IP for client examples */
 
 /**
  * @brief Connection Configuration
  */
 #define MAX_PENDING_CONNECTIONS 5  /**< Maximum pending connections for listen() */
 #define MAX_CLIENTS 10             /**< Maximum number of simultaneous clients */
 #define CONNECTION_TIMEOUT_SEC 10  /**< Connection timeout in seconds */
 
 /**
  * @brief Buffer Configuration
  */
 #define DEFAULT_BUFFER_SIZE 1024   /**< Default buffer size for send/recv */
 #define LARGE_BUFFER_SIZE 8192     /**< Larger buffer for high-throughput applications */
 #define SMALL_BUFFER_SIZE 256      /**< Smaller buffer for constrained systems */
 
 /**
  * @brief Socket Options Configuration
  */
 #define DEFAULT_KEEPALIVE_IDLE 60      /**< Seconds before sending keepalive probes */
 #define DEFAULT_KEEPALIVE_INTERVAL 5    /**< Seconds between keepalive probes */
 #define DEFAULT_KEEPALIVE_COUNT 5       /**< Number of keepalive probes before giving up */
 #define DEFAULT_SOCKET_TIMEOUT_SEC 5    /**< Default socket timeout in seconds */
 
 /**
  * @brief Enable/disable features based on platform capabilities
  * Uncomment the appropriate lines based on your target platform
  */
 #define ENABLE_IPV6               /**< Enable IPv6 support */
 /* #define ENABLE_TLS */           /**< Enable TLS/SSL support */
 /* #define ENABLE_SCTP */          /**< Enable SCTP support */
 #define ENABLE_EPOLL              /**< Enable epoll multiplexing (Linux) */
 /* #define ENABLE_KQUEUE */        /**< Enable kqueue multiplexing (BSD) */
 #define ENABLE_ZERO_COPY          /**< Enable zero-copy optimizations */
 
 /**
  * @brief Platform-specific adjustments
  * These parameters can be adjusted based on the target embedded platform
  */
 #define PLATFORM_NAME "Generic POSIX" /**< Target platform name */
 #define PLATFORM_CPU_MHZ 1000        /**< CPU speed in MHz (for timing calculations) */
 #define PLATFORM_MEMORY_KB 65536     /**< Available memory in KB */
 #define PLATFORM_MAX_SOCKETS 64      /**< Maximum sockets supported by platform */
 
 /**
  * @brief Debug and logging configuration
  */
 /* #define ENABLE_DEBUG_LOGGING */   /**< Enable verbose debug logs */
 #define LOG_TO_SYSLOG               /**< Enable logging to syslog */
 /* #define LOG_TO_FILE */           /**< Enable logging to file */
 #define DEFAULT_LOG_LEVEL 3         /**< Default log level (0-5) */
 
 #endif /* CONFIG_H */