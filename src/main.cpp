#include <iostream>
#include <thread>
#include <vector>

#include "sip_server.hpp"
#include "grpc_service.hpp"
#include "rtp_receiver.hpp"
#include "config.hpp"
#include <thread>

void run_sip_server_thread(AppConfig config) {
    SipServer server;
    server.run(config);
}

int main(int argc, char* argv[]) {
    std::cout << "Starting SIP to gRPC Gateway..." << std::endl;
    
    // Load config
    std::string config_file = "config.properties";
    if (argc > 1) {
        config_file = argv[1];
    }
    
    AppConfig config = ConfigLoader::load(config_file);
    std::cout << "Loaded Config:" << std::endl;
    std::cout << "  Bind IP:   " << config.bind_ip << std::endl;
    std::cout << "  Echo Mode: " << (config.echo_only ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "  SIP Port:  " << config.sip_port << std::endl;
    std::cout << "  gRPC Port: " << config.grpc_port << std::endl;
    std::cout << "  RTP Range: " << config.rtp_min_port << " - " << config.rtp_max_port << std::endl;

    std::thread sip_thread(run_sip_server_thread, config);
    std::thread grpc_thread(run_grpc_server, config);

    sip_thread.join();
    grpc_thread.join();

    return 0;
}
