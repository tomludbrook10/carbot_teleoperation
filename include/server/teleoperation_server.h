#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include "defs.h"
#include "server_streamer.h"

class TeleoperationServer {
public:
    explicit TeleoperationServer(const std::string& server_address,
                                 const std::string& client_address,
                                 const bool is_ros2_node = false,
                                 const int video_port = 5000,
                                 const int rpc_port = 50051);
    ~TeleoperationServer();

    bool IsRunning() const;
    void Stop();
    bool GetNextCommand(CommandRequest& out_command);
    void UpdateKinematics(const Kinematics& kinematics);

private:
    void RunServer(const std::string server_address, const int rpc_port);

    std::mutex cq_mu_;
    std::queue<CommandRequest> cq_;
    std::condition_variable cq_cv_;

    std::mutex k_mu_;
    Kinematics current_kinematics_;
    std::condition_variable k_cv_;
    bool k_updated_ = false;

    std::string server_address_;
    std::string client_address_;

    std::thread rpc_thread_;

    int video_port_;
    ServerStreamer streamer_;

    std::atomic<bool> running_{true};
    const bool is_ros2_node_;
};
