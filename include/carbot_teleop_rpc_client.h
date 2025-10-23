#pragma once

#include <grpcpp/channel.h>
#include <memory>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "carbot_teleop.grpc.pb.h"
#include "defs.h"

class CarbotTeleopRPCClient {
public:
    CarbotTeleopRPCClient(std::shared_ptr<grpc::Channel> channel,
                          std::queue<CommandRequest>* cq,
                          std::mutex* cq_mu,
                          std::condition_variable* cq_cv,
                          std::mutex* k_mu,
                          Kinematics* current_kinematics,
                          std::condition_variable* k_cv,
                          bool* k_updated,
                          std::atomic<bool>* stop_rpc);
    std::string GetStatus();
    void StreamTeleoperation();

private:
    std::unique_ptr<carbot_teleop::CarbotTeleop::Stub> stub_;

    std::queue<CommandRequest>* cq_;
    std::mutex* cq_mu_;
    std::condition_variable* cq_cv_;

    std::mutex* k_mu_;
    Kinematics* current_kinematics_;
    std::condition_variable* k_cv_;
    bool* k_updated_;
    std::atomic<bool>* stop_rpc_;
};
