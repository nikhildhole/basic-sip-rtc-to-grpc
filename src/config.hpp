#pragma once
#include <string>
#include <map>

struct AppConfig {
    std::string bind_ip = "0.0.0.0";
    bool echo_only = false;
    int sip_port = 5060;
    int grpc_port = 50051;
    int rtp_min_port = 10000;
    int rtp_max_port = 20000;
};

class ConfigLoader {
public:
    static AppConfig load(const std::string& filename);
};
