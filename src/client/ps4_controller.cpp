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
    std::cout << std::endl;
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

    bool run = true;
    bool received_input_from_4 = false;
    bool received_input_from_5 = false;
    bool received_input_from_0 = false;

    while (run) {
        SDL_Event e;
        // If we add the delay like we did, wouldn't the queue fill up very fast.
        // Then we would have a delay as it cleans out commands?
        // This could be the reason why there is some times a delayed
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_CONTROLLERAXISMOTION) {
                int axis = (int)e.caxis.axis;
                if (axis == 4 && !received_input_from_4) {
                    b_speed_input = NormaliseAndProject(e.caxis.value, -max_speed_);
                    received_input_from_4 = true;
                } else if (axis == 5 && !received_input_from_5) {
                    f_speed_input = NormaliseAndProject(e.caxis.value, max_speed_);
                    received_input_from_5 = true;
                } else if (axis == 0 && !received_input_from_0) {
                    steering_input = NormaliseAndProject(e.caxis.value, max_steering_angle_);
                    received_input_from_0 = true;
                }
            }
            if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                int button = (int)e.cbutton.button;
                if (button == 1) { // B button to exit
                    run = false;
                    break;
                }
            }

            // runs out of all the events in the queue
            if (received_input_from_0 && received_input_from_4 && received_input_from_5) {
                break;
            }
        }

        if (run == false) {
            break;
        }


        if (f_speed_input > 0 && b_speed_input > 0) {
            f_speed_input = 0.0f;
            b_speed_input = 0.0f;
        }

        if (f_speed_input < 0)
            b_speed_input = 0.0f;
        if (b_speed_input > 0)
            f_speed_input = 0.0f;

        
        CommandRequest cmd{f_speed_input + b_speed_input, -steering_input};
        {
            std::lock_guard<std::mutex> lock(*cq_mu_);
            cq_->push(cmd);
        }
        cq_cv_->notify_one();


        received_input_from_4 = false;
        received_input_from_5 = false;
        received_input_from_0 = false;
        SDL_Delay(10);
        SDL_FlushEvent(SDL_CONTROLLERAXISMOTION);
    }
}
