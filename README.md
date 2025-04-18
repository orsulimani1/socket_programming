# Socket Programming for Real-time Embedded Systems

This repository contains comprehensive examples, utilities, and real-world patterns for implementing socket communication in real-time embedded systems. It covers various socket types, multiplexing techniques, and optimization strategies suitable for resource-constrained environments.

## Overview

The project demonstrates how to use different socket APIs with a special focus on real-time and embedded systems requirements:

- TCP, UDP, and Unix Domain Sockets
- Socket multiplexing techniques (select, poll, epoll)
- Zero-copy data transfer methods
- Non-blocking and asynchronous I/O
- Real-world application examples

## Building the Examples

### Prerequisites

- CMake 3.10 or higher
- C compiler with C11 support
- POSIX-compliant operating system
- Optional: OpenSSL for secure socket examples

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/yourusername/socket-programming.git
cd socket-programming

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
make

# Optional: Run tests
make test

# Optional: Generate documentation
make docs
```

## Project Structure

```
socket_programming/
├── CMakeLists.txt              # Main CMake configuration
├── Doxyfile.in                 # Doxygen configuration template
├── README.md                   # Project documentation
├── include/                    # Header files
│   ├── common.h                # Common utilities and definitions
│   ├── socket_utils.h          # Socket utility functions
│   ├── config.h                # Configuration parameters
│   └── error_handling.h        # Error handling macros and functions
├── src/                        # Source code
│   ├── tcp_sockets/            # TCP socket examples
│   ├── udp_sockets/            # UDP socket examples
│   ├── unix_domain_sockets/    # Unix Domain Socket examples
│   ├── multiplexing/           # Socket multiplexing examples
│   ├── zerocopy/               # Zero-copy implementation examples
│   └── examples/               # Real-world examples
└── examples/                   # Additional example subdirectory
    └── advanced/               # Advanced examples
```

## Core Examples Overview

### TCP Socket Examples

- **tcp_server**: Basic TCP server with listening and connection acceptance
- **tcp_client**: Basic TCP client that connects to a server

### UDP Socket Examples

- **udp_server**: Basic UDP server for connectionless communication
- **udp_client**: Basic UDP client showing datagram transmission
- **udp_broadcast**: Demonstrates sending broadcast messages to multiple receivers

### Unix Domain Socket Examples

- **uds_server**: Local IPC using filesystem-based socket paths
- **uds_client**: Client for Unix Domain Socket communication

### Multiplexing Examples

- **select_server**: Multiple client handling with the portable select() API
- **epoll_server**: High-performance Linux multiplexing using epoll
- **poll_server**: Polling-based I/O multiplexing

### Zero-Copy Examples

- **zero_copy_sendfile**: Efficient file transfer using the sendfile() API
- **zero_copy_mmap**: Memory-mapped I/O for zero-copy transfers
- **splice_example**: Using splice() for data transfer between file descriptors

## Real-World Examples

- **sensor_monitoring**: IoT sensor data collection system using UDP
- **secure_command_server**: Secure remote control system using TLS/SSL
- **high_perf_webserver**: Epoll-based high-concurrency web server
- **low_latency_trading**: Optimized socket communication for latency-critical applications
- **can_automotive**: CAN bus communication for automotive systems

## Advanced Features

- **Non-blocking I/O**: Asynchronous socket communication for real-time applications
- **Socket timeouts**: Reliable communication with timeout handling
- **Error handling**: Robust error detection and recovery strategies
- **Zero-copy optimizations**: Minimizing CPU overhead for data transfers
- **Socket options**: Fine-tuning socket behavior for specific requirements

## Embedded Systems Considerations

This project addresses several key challenges specific to embedded systems:

1. **Resource constraints**: Examples demonstrate memory-efficient implementations
2. **Real-time requirements**: Non-blocking and timeout-based approaches for deterministic behavior
3. **Power efficiency**: Techniques for minimizing power consumption in network operations
4. **Reliability**: Error handling and recovery strategies for robust communication

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request