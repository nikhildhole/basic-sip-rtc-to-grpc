#include <iostream>
#include <string>

#include "sip_server.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <cstring>

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

SipServer::SipServer() : running_(false), socket_fd_(INVALID_SOCKET) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

SipServer::~SipServer() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

void SipServer::run(const AppConfig& config) {
    config_ = config;
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ == INVALID_SOCKET) {
        std::cerr << "Failed to create SIP socket" << std::endl;
        return;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, config_.bind_ip.c_str(), &server_addr.sin_addr);
    server_addr.sin_port = htons(config_.sip_port);

    if (bind(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Failed to bind SIP socket on port " << config_.sip_port << ". Is it already in use?" << std::endl;
        closesocket(socket_fd_);
        return;
    }

    std::cout << "SIP Server listening on UDP " << config_.sip_port << "..." << std::endl;
    running_ = true;

    char buffer[4096];
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (running_) {
        // Simple blocking recvfrom
        int bytes = recvfrom(socket_fd_, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&client_addr, &client_len);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            std::string msg(buffer);
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            
            std::cout << "Received SIP message from " << client_ip << ":" << ntohs(client_addr.sin_port) << std::endl;
            handle_message(msg, std::string(client_ip), ntohs(client_addr.sin_port));
        }
    }
    
    closesocket(socket_fd_);
}

void SipServer::stop() {
    running_ = false;
    // Force close socket to break recv loop (imperfect but simple for now)
    if (socket_fd_ != INVALID_SOCKET) {
        closesocket(socket_fd_);
        socket_fd_ = INVALID_SOCKET;
    }
}

#include <sstream>
#include <map>

struct SipMessage {
    std::string method;
    std::string uri;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
};

SipMessage parse_sip(const std::string& raw) {
    SipMessage msg;
    std::istringstream stream(raw);
    std::string line;
    
    // Request Line
    if (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream req_line(line);
        req_line >> msg.method >> msg.uri >> msg.version;
    }

    // Headers
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break; // End of headers

        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string val = line.substr(colon + 1);
            // Trim whitespace
            while (!val.empty() && val[0] == ' ') val.erase(0, 1);
            msg.headers[key] = val;
        }
    }

    // Body
    std::stringstream body_ss;
    body_ss << stream.rdbuf();
    msg.body = body_ss.str();

    return msg;
}

void SipServer::handle_message(const std::string& raw_msg, const std::string& remote_ip, int remote_port) {
    SipMessage msg = parse_sip(raw_msg);
    
    if (msg.headers.find("Via") == msg.headers.end() || 
        msg.headers.find("From") == msg.headers.end() || 
        msg.headers.find("To") == msg.headers.end() || 
        msg.headers.find("Call-ID") == msg.headers.end() || 
        msg.headers.find("CSeq") == msg.headers.end()) {
        std::cerr << "Dropping invalid SIP message (missing headers)" << std::endl;
        return;
    }

    std::string branch;
    size_t branch_pos = msg.headers["Via"].find("branch=");
    if (branch_pos != std::string::npos) {
        // extract branch
    }

    std::string call_id;
    if (msg.headers.count("Call-ID")) call_id = msg.headers["Call-ID"];

    if (msg.method == "INVITE") {
        std::cout << "Handling INVITE" << std::endl;
        
        // 1. Send 180 Ringing
        std::stringstream ringing;
        ringing << "SIP/2.0 180 Ringing\r\n";
        ringing << "Via: " << msg.headers["Via"] << "\r\n";
        ringing << "From: " << msg.headers["From"] << "\r\n";
        ringing << "To: " << msg.headers["To"] << ";tag=12345\r\n"; // My tag
        ringing << "Call-ID: " << msg.headers["Call-ID"] << "\r\n";
        ringing << "CSeq: " << msg.headers["CSeq"] << "\r\n";
        ringing << "Content-Length: 0\r\n\r\n";
        send_response(remote_ip, remote_port, ringing.str());


        // 2. Start RTP Receiver for this session
        auto rtp = std::make_shared<RtpReceiver>();
        int rtp_port = rtp->start_dynamic(config_.bind_ip, config_.rtp_min_port, config_.rtp_max_port, config_.echo_only);
        
        if (rtp_port > 0) {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            sessions_[call_id] = rtp;
        } else {
             // Error case
             // TODO: Send 500 Internal Server Error?
             rtp_port = 0;
        }

        // 3. Send 200 OK with My SDP
        std::stringstream response;
        std::string my_sdp = 
            "v=0\r\n"
            "o=- " + call_id + " 123 IN IP4 " + config_.bind_ip + "\r\n"
            "s=Talk\r\n"
            "c=IN IP4 " + config_.bind_ip + "\r\n"
            "t=0 0\r\n"
            "m=audio " + std::to_string(rtp_port) + " RTP/AVP 0\r\n"
            "a=rtpmap:0 PCMU/8000\r\n";

        response << "SIP/2.0 200 OK\r\n";
        response << "Via: " << msg.headers["Via"] << "\r\n";
        response << "From: " << msg.headers["From"] << "\r\n";
        response << "To: " << msg.headers["To"] << ";tag=12345\r\n";
        response << "Call-ID: " << msg.headers["Call-ID"] << "\r\n";
        response << "CSeq: " << msg.headers["CSeq"] << "\r\n";
        response << "Contact: <sip:127.0.0.1:5060>\r\n";
        response << "Content-Type: application/sdp\r\n";
        response << "Content-Length: " << my_sdp.length() << "\r\n\r\n";
        response << my_sdp;

        send_response(remote_ip, remote_port, response.str());
        
    } else if (msg.method == "ACK") {
        std::cout << "Call Established (ACK received)" << std::endl;
    } else if (msg.method == "BYE") {
        std::cout << "Handling BYE for " << call_id << std::endl;
        
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            if (sessions_.count(call_id)) {
                sessions_[call_id]->stop();
                sessions_.erase(call_id);
                std::cout << "Released RTP session for " << call_id << std::endl;
            }
        }
        
        std::stringstream response;
        response << "SIP/2.0 200 OK\r\n";
        response << "Via: " << msg.headers["Via"] << "\r\n";
        response << "From: " << msg.headers["From"] << "\r\n";
        response << "To: " << msg.headers["To"] << "\r\n";
        response << "Call-ID: " << msg.headers["Call-ID"] << "\r\n";
        response << "CSeq: " << msg.headers["CSeq"] << "\r\n";
        response << "Content-Length: 0\r\n\r\n";
        send_response(remote_ip, remote_port, response.str());
        
    } else if (msg.method == "REGISTER") {
         // 405
        std::stringstream response;
        response << "SIP/2.0 405 Method Not Allowed\r\n";
        response << "Via: " << msg.headers["Via"] << "\r\n";
        response << "From: " << msg.headers["From"] << "\r\n";
        response << "To: " << msg.headers["To"] << "\r\n";
        response << "Call-ID: " << msg.headers["Call-ID"] << "\r\n";
        response << "CSeq: " << msg.headers["CSeq"] << "\r\n";
        response << "Content-Length: 0\r\n\r\n";
        send_response(remote_ip, remote_port, response.str());
    } else {
        // OPTIONS or UPDATE or others -> 200 OK
        std::stringstream response;
        response << "SIP/2.0 200 OK\r\n";
        response << "Via: " << msg.headers["Via"] << "\r\n";
        response << "From: " << msg.headers["From"] << "\r\n";
        response << "To: " << msg.headers["To"] << ";tag=12345\r\n";
        response << "Call-ID: " << msg.headers["Call-ID"] << "\r\n";
        response << "CSeq: " << msg.headers["CSeq"] << "\r\n";
        response << "Content-Length: 0\r\n\r\n";
        send_response(remote_ip, remote_port, response.str());
    }
}

void SipServer::send_response(const std::string& to_ip, int to_port, const std::string& response) {
    sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    inet_pton(AF_INET, to_ip.c_str(), &dest_addr.sin_addr);
    dest_addr.sin_port = htons(to_port);
    
    sendto(socket_fd_, response.c_str(), response.length(), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
}

