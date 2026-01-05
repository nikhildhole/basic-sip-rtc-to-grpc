#include <iostream>

#include "rtp_receiver.hpp"
#include "grpc_service.hpp"
#include <iostream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

RtpReceiver::RtpReceiver() : running_(false), socket_fd_(INVALID_SOCKET) {
}

RtpReceiver::~RtpReceiver() {
    stop();
}

void RtpReceiver::stop() {
    running_ = false;
    // Close socket to wake up/break receive_loop
    if (socket_fd_ != INVALID_SOCKET) {
        closesocket(socket_fd_);
        socket_fd_ = INVALID_SOCKET;
    }
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
}

int RtpReceiver::start_dynamic(const std::string& bind_ip, int min_port, int max_port, bool echo_mode) {
    if (running_) return -1;
    echo_mode_ = echo_mode;

    for (int port = min_port; port <= max_port; port += 2) { // RTP usually even
        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ == INVALID_SOCKET) {
            continue;
        }

        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        inet_pton(AF_INET, bind_ip.c_str(), &server_addr.sin_addr);
        server_addr.sin_port = htons(port);

        if (bind(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) != SOCKET_ERROR) {
            std::cout << "RTP Receiver bound to " << bind_ip << ":" << port << (echo_mode ? " [ECHO ONLY]" : " [gRPC]") << std::endl;
            running_ = true;
            receive_thread_ = std::thread(&RtpReceiver::receive_loop, this);
            return port;
        }
        
        closesocket(socket_fd_);
    }
    
    std::cerr << "Failed to find free RTP port in range " << min_port << "-" << max_port << std::endl;
    return -1;
}

void RtpReceiver::receive_loop() {
    char buffer[4096];
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Loop
    while (running_) {
        int bytes = recvfrom(socket_fd_, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &client_len);
        if (bytes > 0) {
            if (echo_mode_) {
                // Echo back to source ONLY
                sendto(socket_fd_, buffer, bytes, 0, (struct sockaddr*)&client_addr, client_len);
            } else {
                // Broadcast to gRPC ONLY (no echo)
                 broadcast_audio(buffer, bytes);
            }
        }
    }
}

