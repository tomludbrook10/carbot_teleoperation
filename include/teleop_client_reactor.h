#pragma once

#include <grpcpp/client_context.h>
#include <mutex>
#include <queue>
#include <condition_variable>
#include "carbot_teleop.grpc.pb.h"
#include "defs.h"

class TeleopClientReactor : public grpc::ClientBidiReactor<carbot_teleop::CommandRequest, carbot_teleop::CurrentKinematics> {
public:
    explicit TeleopClientReactor(carbot_teleop::CarbotTeleop::Stub* stub,
                                 std::queue<CommandRequest>* cq,
                                 std::mutex* cq_mu,
                                 std::condition_variable* cq_cv,
                                 std::mutex* k_mu,
                                 Kinematics* current_kinematics,
                                 std::condition_variable* k_cv,
                                 bool* k_updated);

    void OnWriteDone(bool ok) override;
    void OnReadDone(bool ok) override;
    void OnDone(const grpc::Status& status) override;
    void Stop();
    grpc::Status Await();

private:
    void NextWrite();

    carbot_teleop::CarbotTeleop::Stub* stub_;
    grpc::ClientContext context_;

    carbot_teleop::CurrentKinematics server_kinematics_;
    carbot_teleop::CommandRequest command_;

    std::queue<CommandRequest>* cq_;
    std::mutex* cq_mu_;
    std::condition_variable* cq_cv_;

    std::mutex* k_mu_;
    Kinematics* current_kinematics_;
    std::condition_variable* k_cv_;
    bool* k_updated_;

    grpc::Status status_;
    std::mutex mu_;
    std::condition_variable cv_;
    bool done_ = false;
};
