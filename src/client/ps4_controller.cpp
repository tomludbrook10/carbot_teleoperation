#include "ps4_controller.h"
#include <SDL2/SDL.h>
#include <iostream>


PS4Controller::PS4Controller(std::queue<CommandRequest>* cq, std::mutex* mu, std::condition_variable* cv,
                               std::mutex* k_mu, Kinematics* current_kinematics,
                               std::condition_variable* k_cv, bool* k_updated)
    : cq_(cq), cq_mu_(mu), cq_cv_(cv), k_mu_(k_mu), current_kinematics_(current_kinematics),
      k_cv_(k_cv), k_updated_(k_updated) {}

PS4Controller::~PS4Controller() {
    run_listener_.store(false, std::memory_order_relaxed);
    k_cv_->notify_one();

    if (kinematic_listener_thread_.joinable()) {
        kinematic_listener_thread_.join();
    }
}

void PS4Controller::KinematicListener() {
    while (run_listener_.load(std::memory_order_relaxed)) {
        std::unique_lock<std::mutex> lock(*k_mu_);
        k_cv_->wait(lock, [this]{ return *k_updated_ || !run_listener_.load(std::memory_order_relaxed); });
        if (!run_listener_.load(std::memory_order_relaxed)) {
            break;
        }
        current_kinematics_->Print();
        *k_updated_ = false;
    }
}

float NormaliseAndProject(int value, float max) {
    if (abs(value) < 2000) return 0.0f; // deadzone
    float normalized = value / 32767.0f; // normalize to -1 to 1
    return normalized * max;
}

void PS4Controller::Run() {
    SDL_Init(SDL_INIT_GAMECONTROLLER);
    SDL_GameController *ctrl = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            ctrl = SDL_GameControllerOpen(i);
            std::cout << "Opened: " << SDL_GameControllerName(ctrl) << "\n";
            break;
        } else {
            std::cout << "No PS4 controller found.\n";
            return;
        }
    }

    kinematic_listener_thread_ = std::thread(&PS4Controller::KinematicListener, this);

    float f_speed_input = 0.0f;
    float b_speed_input = 0.0f;
    float steering_input = 0.0f;
    float f_speed_input_prev = 0.0f;
    float b_speed_input_prev = 0.0f;
    float steering_input_prev = 0.0f;

    bool run = true;

    int i = 0;

    while (run) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_CONTROLLERAXISMOTION) {
                int axis = (int)e.caxis.axis;
                if (axis == 4) {
                    b_speed_input = NormaliseAndProject(e.caxis.value, -max_speed_);
                } else if (axis == 5) {
                    f_speed_input = NormaliseAndProject(e.caxis.value, max_speed_);
                } else if (axis == 0) {
                    steering_input = NormaliseAndProject(e.caxis.value, max_steering_angle_);
                }
            }
            if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                int button = (int)e.cbutton.button;
                if (button == 1) { // B button to exit
                    run = false;
                    break;
                }
            }

            if (f_speed_input > 0 && b_speed_input > 0) {
                f_speed_input = 0.0f;
                b_speed_input = 0.0f;
            }

            if (f_speed_input < 0)
                b_speed_input = 0.0f;
            if (b_speed_input > 0)
                f_speed_input = 0.0f;

            //steering_input = -steering_input; // invert steering
            if (f_speed_input == f_speed_input_prev &&
                b_speed_input == b_speed_input_prev &&
                steering_input == steering_input_prev) {
                continue; // no change
            }
            
            CommandRequest cmd{f_speed_input + b_speed_input, -steering_input};
            {
                std::lock_guard<std::mutex> lock(*cq_mu_);
                cq_->push(cmd);
            }
            cq_cv_->notify_one();
            f_speed_input_prev = f_speed_input;
            b_speed_input_prev = b_speed_input;
            steering_input_prev = steering_input;
        }
        SDL_Delay(10);
    }
    SDL_Delay(10);
}
