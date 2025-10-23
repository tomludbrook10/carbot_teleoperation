#pragma once

#include <grpcpp/server.h>
#include <grpcpp/server_context.h>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include "carbot_teleop.grpc.pb.h"
#include "defs.h"

class CarbotTeleopServiceImpl final : public carbot_teleop::CarbotTeleop::CallbackService {
public:
    explicit CarbotTeleopServiceImpl(std::queue<CommandRequest>* cq, std::mutex* cq_mu, std::condition_variable* cq_cv,
                                     Kinematics* kinematics, std::mutex* k_mu, std::condition_variable* k_cv,
                                     std::atomic<bool>* running);

    grpc::ServerUnaryReactor* GetStatus(grpc::CallbackServerContext* context,
                                        const google::protobuf::Empty* request,
                                        carbot_teleop::StatusResponse* response) override;

    grpc::ServerBidiReactor<carbot_teleop::CommandRequest, carbot_teleop::CurrentKinematics>* StreamTeleoperation(
        grpc::CallbackServerContext* context) override;

private:
    std::mutex* cq_mu_;
    std::queue<CommandRequest>* cq_;
    std::condition_variable* cq_cv_;

    std::mutex* k_mu_;
    Kinematics* kinematics_;
    std::condition_variable* k_cv_;

    std::atomic<bool>* running_;
};
