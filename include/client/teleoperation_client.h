#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include "defs.h"
#include "client_streamer.h"

class TeleoperationClient {
public:
    explicit TeleoperationClient(const std::string& server_address,
                                 const std::string& client_address,
                                 const int video_port = 5000,
                                 const int rpc_port = 50051);
    ~TeleoperationClient();
    void Start();

private:
    void RunRPCClient(std::string server_address, int rpc_port);

    std::string server_address_;
    std::string client_address_;
    int video_port_;
    std::thread rpc_thread_;

    std::queue<CommandRequest> cq_;
    std::mutex cq_mu_;
    std::condition_variable cq_cv_;

    std::mutex k_mu_;
    Kinematics current_kinematics_;
    std::condition_variable k_cv_;
    bool k_updated_ = false;

    std::atomic<bool> stop_rpc_{false};
};
