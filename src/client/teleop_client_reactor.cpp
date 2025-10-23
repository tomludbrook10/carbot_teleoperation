#include "teleop_client_reactor.h"
#include <iostream>

// Helper function to convert proto to Kinematics
Kinematics MakeKinematics(const carbot_teleop::CurrentKinematics& proto_kinematics) {
    Kinematics kinematics;
    kinematics.current_speed = proto_kinematics.current_speed();
    kinematics.current_steering_angle = proto_kinematics.current_steering_angle();
    const auto& proto_pose = proto_kinematics.pose();
    kinematics.pose.position.x = proto_pose.position().x();
    kinematics.pose.position.y = proto_pose.position().y();
    kinematics.pose.orientation = proto_pose.orientation();
    return kinematics;
}

TeleopClientReactor::TeleopClientReactor(carbot_teleop::CarbotTeleop::Stub* stub,
                                         std::queue<CommandRequest>* cq,
                                         std::mutex* cq_mu,
                                         std::condition_variable* cq_cv,
                                         std::mutex* k_mu,
                                         Kinematics* current_kinematics,
                                         std::condition_variable* k_cv,
                                         bool* k_updated)
    : stub_(stub), cq_(cq), cq_mu_(cq_mu), cq_cv_(cq_cv),
      k_mu_(k_mu), current_kinematics_(current_kinematics), k_cv_(k_cv), k_updated_(k_updated) {
    stub_->async()->StreamTeleoperation(&context_, this);
    NextWrite();
    StartRead(&server_kinematics_);
    StartCall(); // Activates the RPC.
}

void TeleopClientReactor::OnWriteDone(bool ok) {
    if (!ok) {
        std::cerr << "Write failed, ending stream." << std::endl;
        StartWritesDone();
        return;
    }
    NextWrite();
}

void TeleopClientReactor::OnReadDone(bool ok) {
    if (ok) {
        {
            std::unique_lock<std::mutex> lock(*k_mu_);
            *current_kinematics_ = MakeKinematics(server_kinematics_);
            *k_updated_ = true;
            k_cv_->notify_one();
        }
        StartRead(&server_kinematics_);
    }
}

void TeleopClientReactor::Stop() {
    std::cout << "Cancelling TeleopClientReactor" << std::endl;
    context_.TryCancel();
}

void TeleopClientReactor::OnDone(const grpc::Status& status) {
    std::unique_lock<std::mutex> lock(mu_);
    status_ = status;
    done_ = true;
    cv_.notify_one();
}

grpc::Status TeleopClientReactor::Await() {
    std::unique_lock<std::mutex> lock(mu_);
    cv_.wait(lock, [this] { return done_; });
    return std::move(status_);
}

void TeleopClientReactor::NextWrite() {
    std::unique_lock<std::mutex> lock(*cq_mu_);
    cq_cv_->wait(lock, [this]{ return !cq_->empty(); });
    const auto& command = cq_->front();
    command_.set_speed(command.speed);
    command_.set_steering_angle(command.steering_angle);
    cq_->pop();
    StartWrite(&command_);
}
