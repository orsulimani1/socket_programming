/**
 * @file error_handling.h
 * @brief Error handling utilities for socket programming
 * 
 * This header provides macros and functions for consistent error handling
 * across socket programming examples. It includes tools for logging,
 * exit handling, and translating errno values to meaningful messages.
 */

#ifndef ERROR_HANDLING_H
#define ERROR_HANDLING_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

/**
 * Log levels for the logging system
 */
typedef enum {
    LOG_FATAL = 0,
    LOG_ERROR = 1,
    LOG_WARN  = 2,
    LOG_INFO  = 3,
    LOG_DEBUG = 4,
    LOG_TRACE = 5
} log_level_t;

#ifndef _CURRENT_LEVEL
/** Current log level - can be changed at runtime */
log_level_t current_log_level;
/** Set this to 0 to disable color output */
int use_color_output = 1;
#else
extern log_level_t current_log_level;
extern int use_color_output;
#endif

/**
 * @brief Set the current log level
 * 
 * @param level New log level
 */
static inline void set_log_level(log_level_t level) {
    current_log_level = level;
}

/**
 * @brief Log a message with the specified level
 * 
 * @param level Log level for this message
 * @param file Source file name (usually __FILE__)
 * @param line Line number (usually __LINE__)
 * @param func Function name (usually __func__)
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
static inline void log_message(log_level_t level, const char *file, 
                            int line, const char *func, 
                            const char *fmt, ...) {
    if (level > current_log_level) {
        return;
    }
    
    // Level prefixes with optional colors
    static const char *level_strings_color[] = {
        "\033[1;31m[FATAL]\033[0m", "\033[31m[ERROR]\033[0m", 
        "\033[33m[WARN]\033[0m", "\033[32m[INFO]\033[0m", 
        "\033[36m[DEBUG]\033[0m", "\033[35m[TRACE]\033[0m"
    };
    
    static const char *level_strings[] = {
        "[FATAL]", "[ERROR]", "[WARN]", "[INFO]", "[DEBUG]", "[TRACE]"
    };
    
    const char **strings = use_color_output ? level_strings_color : level_strings;
    
    // Print header with file, line, and function info for DEBUG and above
    if (level >= LOG_DEBUG) {
        fprintf(stderr, "%s %s:%d (%s): ", 
                strings[level], file, line, func);
    } else {
        fprintf(stderr, "%s ", strings[level]);
    }
    
    // Print the actual message
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    
    // Add newline if not already present
    if (fmt[strlen(fmt)-1] != '\n') {
        fprintf(stderr, "\n");
    }
}

/**
 * @brief Log an error message with the current errno value
 * 
 * @param file Source file name (usually __FILE__)
 * @param line Line number (usually __LINE__)
 * @param func Function name (usually __func__)
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
static inline void log_errno(const char *file, int line, 
                            const char *func, const char *fmt, ...) {
    if (LOG_ERROR > current_log_level) {
        return;
    }
    
    // Store errno immediately to prevent changes
    int err = errno;
    
    // Level prefix with optional color
    const char *prefix = use_color_output ? "\033[31m[ERROR]\033[0m" : "[ERROR]";
        
    // Print header
    fprintf(stderr, "%s ", prefix);
    
    // Print the actual message
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    
    // Add the errno information
    fprintf(stderr, ": %s (errno=%d)\n", strerror(err), err);
}

/**
 * @brief Get a descriptive string for common socket errors
 * 
 * @param err Error number (errno)
 * @return Descriptive string for the error
 */
static inline const char* socket_error_string(int err) {
    switch (err) {
        case EACCES:
            return "Permission denied";
        case EADDRINUSE:
            return "Address already in use";
        case EADDRNOTAVAIL:
            return "Address not available";
        case EAFNOSUPPORT:
            return "Address family not supported";
        case EAGAIN:
            return "Resource temporarily unavailable (try again)";
        case EALREADY:
            return "Connection already in progress";
        case EBADF:
            return "Bad file descriptor";
        case ECONNABORTED:
            return "Connection aborted";
        case ECONNREFUSED:
            return "Connection refused";
        case ECONNRESET:
            return "Connection reset by peer";
        case EDESTADDRREQ:
            return "Destination address required";
        case EFAULT:
            return "Bad address";
        case EHOSTUNREACH:
            return "Host is unreachable";
        case EINPROGRESS:
            return "Operation now in progress";
        case EINTR:
            return "Interrupted function call";
        case EINVAL:
            return "Invalid argument";
        case EISCONN:
            return "Socket is already connected";
        case EMFILE:
            return "Too many open files";
        case EMSGSIZE:
            return "Message too large";
        case ENETDOWN:
            return "Network is down";
        case ENETRESET:
            return "Connection aborted by network";
        case ENETUNREACH:
            return "Network unreachable";
        case ENOBUFS:
            return "No buffer space available";
        case ENOPROTOOPT:
            return "Protocol not available";
        case ENOTCONN:
            return "Socket not connected";
        case ENOTSOCK:
            return "Not a socket";
        case EOPNOTSUPP:
            return "Operation not supported";
        case EPROTO:
            return "Protocol error";
        case EPROTONOSUPPORT:
            return "Protocol not supported";
        case EPROTOTYPE:
            return "Wrong protocol type for socket";
        case ETIMEDOUT:
            return "Connection timed out";
        default:
            return strerror(err);
    }
}

/* Convenience macros for logging */
#define LOG_FATAL(fmt, ...) \
    log_message(LOG_FATAL, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
    
#define LOG_ERROR(fmt, ...) \
    log_message(LOG_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
    
#define LOG_WARN(fmt, ...) \
    log_message(LOG_WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
    
#define LOG_INFO(fmt, ...) \
    log_message(LOG_INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
    
#define LOG_DEBUG(fmt, ...) \
    log_message(LOG_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
    
#define LOG_TRACE(fmt, ...) \
    log_message(LOG_TRACE, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_ERRNO(fmt, ...) \
    log_errno(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * @brief Fatal error handler
 * 
 * Logs the error and exits the program.
 * 
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
#define FATAL(fmt, ...) do { \
    LOG_FATAL(fmt, ##__VA_ARGS__); \
    exit(EXIT_FAILURE); \
} while (0)

/**
 * @brief Fatal error handler with errno information
 * 
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
#define FATAL_ERRNO(fmt, ...) do { \
    LOG_ERRNO(fmt, ##__VA_ARGS__); \
    exit(EXIT_FAILURE); \
} while (0)

/**
 * @brief Check a condition and exit with error message if it fails
 * 
 * @param cond Condition to check
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
#define CHECK(cond, fmt, ...) do { \
    if (!(cond)) { \
        FATAL(fmt, ##__VA_ARGS__); \
    } \
} while (0)

/**
 * @brief Check a condition and exit with errno information if it fails
 * 
 * @param cond Condition to check
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
#define CHECK_ERRNO(cond, fmt, ...) do { \
    if (!(cond)) { \
        FATAL_ERRNO(fmt, ##__VA_ARGS__); \
    } \
} while (0)

/**
 * @brief Check if a socket API call succeeds
 * 
 * @param call Socket API call to check
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
#define SOCKET_CHECK(call, fmt, ...) do { \
    if ((call) < 0) { \
        FATAL_ERRNO(fmt, ##__VA_ARGS__); \
    } \
} while (0)

#endif /* ERROR_HANDLING_H */