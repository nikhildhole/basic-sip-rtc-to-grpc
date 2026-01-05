#include "config.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

AppConfig ConfigLoader::load(const std::string& filename) {
    AppConfig config;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open " << filename << ". Using defaults." << std::endl;
        return config;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Remove comments
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        // Trim
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty()) continue;

        std::istringstream is_line(line);
        std::string key;
        if (std::getline(is_line, key, '=')) {
            std::string value;
            if (std::getline(is_line, value)) {
                // Trim key and value
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                
                try {
                    if (key == "bind_ip") config.bind_ip = value;
                    else if (key == "echo_only") config.echo_only = (value == "true" || value == "1");
                    else if (key == "sip_port") config.sip_port = std::stoi(value);
                    else if (key == "grpc_port") config.grpc_port = std::stoi(value);
                    else if (key == "rtp_min_port") config.rtp_min_port = std::stoi(value);
                    else if (key == "rtp_max_port") config.rtp_max_port = std::stoi(value);
                } catch (...) {
                    std::cerr << "Warning: Invalid value for " << key << ": " << value << std::endl;
                }
            }
        }
    }
    
    return config;
}
