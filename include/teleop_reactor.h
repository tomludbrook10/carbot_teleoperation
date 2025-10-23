#pragma once

#include <grpcpp/server.h>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include "carbot_teleop.grpc.pb.h"
#include "defs.h"

class TeleopReactor : public grpc::ServerBidiReactor<carbot_teleop::CommandRequest, carbot_teleop::CurrentKinematics> {
public:
    TeleopReactor(std::queue<CommandRequest>* cq, std::mutex* cq_mu, std::condition_variable* cq_cv,
                  Kinematics* kinematics, std::mutex* k_mu, std::condition_variable* k_cv,
                  std::atomic<bool>* running);

    void OnWriteDone(bool ok) override;
    void OnReadDone(bool ok) override;
    void OnDone() override;
    void OnCancel() override;

private:
    void NextWrite();

    carbot_teleop::CommandRequest command_request_;
    carbot_teleop::CurrentKinematics current_kinematics_;
    Kinematics prev_kinematics_;

    std::mutex* cq_mu_;
    std::queue<CommandRequest>* cq_;
    std::condition_variable* cq_cv_;

    std::mutex* k_mu_;
    Kinematics* kinematics_;
    std::condition_variable* k_cv_;

    std::atomic<bool>* running_;
};
