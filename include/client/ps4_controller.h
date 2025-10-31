#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include "defs.h"

class PS4Controller {
public:
    explicit PS4Controller(std::queue<CommandRequest>* cq, std::mutex* mu, std::condition_variable* cv,
                           std::mutex* k_mu, Kinematics* current_kinematics,
                           std::condition_variable* k_cv, bool* k_updated);
    ~PS4Controller();
    void Run();

private:
    void KinematicListener();
    void PS4Listener();

    float max_speed_ = 2.0f; // m/s
    float min_speed_ = -2.0f;
    float max_steering_angle_ = 0.436332f; // 25 degrees in radians
    float min_steering_angle_ = -0.436332f;

    std::queue<CommandRequest>* cq_;
    std::mutex* cq_mu_;
    std::condition_variable* cq_cv_;

    std::mutex* k_mu_;
    Kinematics* current_kinematics_;
    std::condition_variable* k_cv_;
    bool* k_updated_;

    std::thread kinematic_listener_thread_;
    std::atomic<bool> run_listener_{true};

    std::atomic<float> speed_input_{0.0f};
    std::atomic<float> steering_input_{0.0f};
    std::atomic<bool> run{true};
};
