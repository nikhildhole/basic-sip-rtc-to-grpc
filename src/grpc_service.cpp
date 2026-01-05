#include <iostream>
#include <string>
#include <grpcpp/grpcpp.h>
#include "audio_gateway.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using audio_gateway::AudioService;
using audio_gateway::Empty;
using audio_gateway::AudioChunk;

#include <iostream>
#include <string>
#include <mutex>
#include <vector>
#include <set>
#include <queue>
#include <condition_variable>
#include <grpcpp/grpcpp.h>
#include "audio_gateway.grpc.pb.h"
#include "grpc_service.hpp"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using audio_gateway::AudioService;
using audio_gateway::Empty;
using audio_gateway::AudioChunk;

// Simple thread-safe queue for each client, or just broadcasting
// For simplicity, we'll keep a list of active Writers and write immediately (locking).
// Note: In production, use a queue per client or a ring buffer to avoid blocking the receiver.

class AudioServiceImpl final : public AudioService::Service {
public:
    Status SubscribeAudio(ServerContext* context, const Empty* request, grpc::ServerWriter<AudioChunk>* writer) override {
        std::cout << "New gRPC Client Subscribed" << std::endl;
        
        std::unique_lock<std::mutex> lock(clients_mutex_);
        clients_.insert(writer);
        lock.unlock();

        // Keep connection open until client disconnects or server stops
        // In a real app we need a way to detect disconnect cleanly, 
        // usually via checking if Write fails or waiting on a condition variable that is never set, 
        // or a "done" context.
        
        // Block here? No, if we block we can't write?
        // Wait, ServerWriter usage: The method returns when the stream is done.
        // So we Must block here while the stream is active.
        
        // Hack: Wait on a CV until shutdown, but we need to remove writer on exit
        // Actually, we can't easily "Write" from another thread if we don't have control here.
        // But gRPC C++ async or sync? This is sync server.
        // In Sync server, we hold the `writer` pointer here. We can use it from other threads IF 
        // we ensure life-time validity. 
        // BUT `SubscribeAudio` blocking is required to keep the stream alive.
        
        // Better: Queue based approach.
        // Each client has a queue. We pop from queue and write to `writer`.
        // `broadcast_audio` pushes to all queues.
        
        ClientQueue my_queue;
        
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            active_queues_.push_back(&my_queue);
        }
        
        while (!context->IsCancelled()) {
            std::vector<char> data = my_queue.pop(); // Blocks until data
            if (data.empty()) break; // shutdown signal
            
            AudioChunk chunk;
            chunk.set_data(data.data(), data.size());
            chunk.set_timestamp(0); // TODO
            
            if (!writer->Write(chunk)) {
                break;
            }
        }
        
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            // remove my_queue
            for (auto it = active_queues_.begin(); it != active_queues_.end(); ++it) {
                if (*it == &my_queue) {
                    active_queues_.erase(it);
                    break;
                }
            }
        }
        
        std::cout << "gRPC Client Disconnected" << std::endl;
        return Status::OK;
    }

    struct ClientQueue {
        std::queue<std::vector<char>> q;
        std::mutex m;
        std::condition_variable cv;
        
        std::vector<char> pop() {
            std::unique_lock<std::mutex> lock(m);
            cv.wait(lock, [this]{ return !q.empty(); });
            auto val = q.front();
            q.pop();
            return val;
        }
        
        void push(const char* data, size_t len) {
            std::lock_guard<std::mutex> lock(m);
            q.push(std::vector<char>(data, data+len));
            cv.notify_one();
        }
    };
    
    static std::vector<ClientQueue*> active_queues_;
    static std::mutex clients_mutex_;
    static std::set<grpc::ServerWriter<AudioChunk>*> clients_; // Dropped in favor of queue
};

std::vector<AudioServiceImpl::ClientQueue*> AudioServiceImpl::active_queues_;
std::mutex AudioServiceImpl::clients_mutex_;
std::set<grpc::ServerWriter<AudioChunk>*> AudioServiceImpl::clients_;

void broadcast_audio(const char* data, size_t len) {
    std::lock_guard<std::mutex> lock(AudioServiceImpl::clients_mutex_);
    for (auto* q : AudioServiceImpl::active_queues_) {
        q->push(data, len);
    }
}

void run_grpc_server(const AppConfig& config) {
    std::string server_address(config.bind_ip + ":" + std::to_string(config.grpc_port));
    AudioServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "gRPC Server listening on " << server_address << std::endl;
    server->Wait();
}

