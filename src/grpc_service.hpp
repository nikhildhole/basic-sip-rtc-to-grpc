#pragma once
#include <string>
#include "config.hpp"

void run_grpc_server(const AppConfig& config);
void broadcast_audio(const char* data, size_t len);

