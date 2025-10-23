#include "teleoperation_server.h"
#include "carbot_teleop_rpc_server.h"
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/security/server_credentials.h>
#include <iostream>

/*
Having a blocking-queue, is slighly low frequency (1 entry every ~10-50ms) command that is then notified
when there is an element in the queue is slighly redundant. The same can be achieved without
a queue. However, it will translated nicely when higher frequency and non-blocking queue.
*/

/*
Things to think about.
How is memory being allocated for the commands and kenmatic values.
Is that being transfered over? Like does queue now own that when we add it it?
*/

TeleoperationServer::TeleoperationServer(const std::string& server_address,
                                         const std::string& client_address,
                                         const bool is_ros2_node,
                                         const int video_port,
                                         const int rpc_port)
    : server_address_(server_address), client_address_(client_address),
      video_port_(video_port),
      streamer_(client_address, video_port),
      is_ros2_node_(is_ros2_node) {
    rpc_thread_ = std::thread(&TeleoperationServer::RunServer, this, server_address_, rpc_port);
    streamer_.setup();
    streamer_.run_async();
}

TeleoperationServer::~TeleoperationServer() {
    Stop();

    if (rpc_thread_.joinable()) {
        rpc_thread_.join();
        std::cout << "RPC thread joined successfully." << std::endl;
    }
}

bool TeleoperationServer::IsRunning() const {
    return running_.load(std::memory_order_relaxed);
}

void TeleoperationServer::Stop() {
    running_.store(false, std::memory_order_relaxed);
    cq_cv_.notify_one();
    k_cv_.notify_one();
}

bool TeleoperationServer::GetNextCommand(CommandRequest& out_command) {
    std::unique_lock<std::mutex> lock(cq_mu_);
    cq_cv_.wait(lock, [this]{ return !cq_.empty() || !running_.load(std::memory_order_relaxed); });
    if (!running_.load(std::memory_order_relaxed)) {
        return false;
    }
    out_command = cq_.front();
    cq_.pop();
    return true;
}

void TeleoperationServer::UpdateKinematics(const Kinematics& kinematics) {
    {
        std::unique_lock<std::mutex> lock(k_mu_);
        current_kinematics_ = kinematics;
        k_updated_ = true;
    }
    k_cv_.notify_one();
}

void TeleoperationServer::RunServer(const std::string server_address, const int rpc_port) {
    CarbotTeleopServiceImpl service(&cq_, &cq_mu_, &cq_cv_, &current_kinematics_, &k_mu_, &k_cv_, &running_, &k_updated_);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address + ":" + std::to_string(rpc_port), grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

    std::thread signal_thread([&server, this]() {
        while (running_.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (server) {
            server->Shutdown();
        }
    });

    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();

    if (signal_thread.joinable()) {
        signal_thread.join();
    }
    std::cout << "Server shut down successfully." << std::endl;
}
