#pragma once
#include <string>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include "rtp_receiver.hpp"
#include "config.hpp"

class SipServer {
public:
    SipServer();
    ~SipServer();
    void run(const AppConfig& config);
    void stop();

private:
    std::atomic<bool> running_;
    int socket_fd_;
    AppConfig config_;
    
    // Map Call-ID -> RtpReceiver
    std::map<std::string, std::shared_ptr<RtpReceiver>> sessions_;
    std::mutex sessions_mutex_;

    void handle_message(const std::string& msg, const std::string& remote_ip, int remote_port);
    void send_response(const std::string& to_ip, int to_port, const std::string& response);
};
