#include "teleop_reactor.h"
#include <iostream>

// Helper function to convert CommandRequest
CommandRequest MakeRequest(carbot_teleop::CommandRequest& command_request) {
    CommandRequest out;
    out.speed = command_request.speed();
    out.steering_angle = command_request.steering_angle();
    return out;
}

// Helper function to convert Kinematics to proto
carbot_teleop::Point MakePoint(double x, double y) {
    carbot_teleop::Point point;
    point.set_x(x);
    point.set_y(y);
    return point;
}

carbot_teleop::Pose MakePose(double x, double y, double orientation) {
    carbot_teleop::Pose pose;
    pose.mutable_position()->CopyFrom(MakePoint(x, y));
    pose.set_orientation(orientation);
    return pose;
}

carbot_teleop::CurrentKinematics MakeKinematics(double x, double y, double orientation,
                                               double speed, double steering_angle) {
    carbot_teleop::CurrentKinematics kinematics;
    kinematics.mutable_pose()->CopyFrom(MakePose(x, y, orientation));
    kinematics.set_current_speed(speed);
    kinematics.set_current_steering_angle(steering_angle);
    return kinematics;
}

carbot_teleop::CurrentKinematics MakeKinematics(const Kinematics& kinematics) {
    return MakeKinematics(
        kinematics.pose.position.x,
        kinematics.pose.position.y,
        kinematics.pose.orientation,
        kinematics.current_speed,
        kinematics.current_steering_angle
    );
}

TeleopReactor::TeleopReactor(std::queue<CommandRequest>* cq, std::mutex* cq_mu, std::condition_variable* cq_cv,
                             Kinematics* kinematics, std::mutex* k_mu, std::condition_variable* k_cv,
                             std::atomic<bool>* running, bool* k_updated)
    : cq_(cq), cq_mu_(cq_mu), cq_cv_(cq_cv),
      kinematics_(kinematics), k_mu_(k_mu), k_cv_(k_cv),
      running_(running), k_updated_(k_updated) {
    StartWrite(&current_kinematics_); // Initial dummy write to kick things off.
    StartRead(&command_request_);
}

void TeleopReactor::OnWriteDone(bool ok) {
    if (!ok) {
        Finish(grpc::Status(grpc::StatusCode::UNKNOWN, "Write failed"));
        return;
    }
    NextWrite();
}

void TeleopReactor::OnReadDone(bool ok) {
    if (ok) {
        {
            std::unique_lock<std::mutex> lock(*cq_mu_);
            cq_->push(MakeRequest(command_request_));
            cq_cv_->notify_one(); // notify the robot thread to check the queue.
        }
        StartRead(&command_request_);
    }
}

void TeleopReactor::OnDone() {
    std::cout << "Teleoperation stream closed." << std::endl;
    delete this;
}

void TeleopReactor::OnCancel() {
    std::cerr << "Teleoperation stream cancelled" << std::endl;
}

void TeleopReactor::NextWrite() {
    {
        std::unique_lock<std::mutex> lock(*k_mu_);
        // wait till there are new kinematics to display.
        k_cv_->wait(lock, [this]{return *k_updated_ || !running_->load(std::memory_order_relaxed); });
        if (!running_->load(std::memory_order_relaxed)) {
            std::cout << "TeleopReactor stopping write due to shutdown." << std::endl;
            Finish(grpc::Status::OK);
            return;
        }
        current_kinematics_ = MakeKinematics(*kinematics_);
        *k_updated_ = false;
        
    }
    StartWrite(&current_kinematics_);
}
