#pragma once
#include <atomic>
#include <vector>
#include <cstdint>
#include <thread>

class RtpReceiver {
public:
    RtpReceiver();
    ~RtpReceiver();
    // Tries to bind to a port in range [min, max] on specified interface. Returns bound port or -1.
    int start_dynamic(const std::string& bind_ip, int min_port, int max_port, bool echo_mode);
    void stop();

private:
    std::atomic<bool> running_;
    int socket_fd_;
    std::thread receive_thread_;
    bool echo_mode_ = false;
    
    void receive_loop();
};
