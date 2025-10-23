#include "keyboard_controller.h"
#include <iostream>
#include <termios.h>
#include <unistd.h>

KeyboardController::KeyboardController(std::queue<CommandRequest>* cq, std::mutex* mu, std::condition_variable* cv,
                                       std::mutex* k_mu, Kinematics* current_kinematics,
                                       std::condition_variable* k_cv, bool* k_updated)
    : cq_(cq), cq_mu_(mu), cq_cv_(cv), k_mu_(k_mu), current_kinematics_(current_kinematics),
      k_cv_(k_cv), k_updated_(k_updated) {}

KeyboardController::~KeyboardController() {
    run_listener_.store(false, std::memory_order_relaxed);
    k_cv_->notify_one();

    if (kinematic_listener_thread_.joinable()) {
        kinematic_listener_thread_.join();
    }
}

void KeyboardController::Run() {
    std::cout << "Starting Kinematic Listener" << std::endl;
    kinematic_listener_thread_ = std::thread(&KeyboardController::KinematicListener, this);

    std::cout << "Press any key (q to quit): " << std::endl;

    // store the old STDIN terminal attributes in old.
    struct termios old = {0};
    if (tcgetattr(STDIN_FILENO, &old) < 0)
        perror("tcgetattr");

    // Turn off cannoical mode and echo.
    struct termios newt = old;
    newt.c_lflag &= ~(ICANON | ECHO); // turns off input is being buffer on the screen and that echo turns off view on the terminal.
    if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) < 0) // specify TCSANOW, to set the attributed now.
        perror("tcsetattr ICANON");

    char c = 0;
    while (true) {
        if (read(STDIN_FILENO, &c, 1) < 0) {
            perror("read");
            break;
        }
        if (c == 'q') {
            std::cout << "Cancelled" << std::endl;
            break;
        }
        SendCommand(c);
    }
    tcsetattr(STDIN_FILENO, TCSADRAIN, &old);
}

void KeyboardController::KinematicListener() {
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

void KeyboardController::SendCommand(const char c) {
    if (c == 'w') {
        current_speed_ += speed_increment_;
    } else if (c == 's') {
        current_speed_ -= speed_increment_;
    } else if (c == 'a') {
        current_steering_angle_ += steering_increment_;
    } else if (c == 'd') {
        current_steering_angle_ -= steering_increment_;
    } else {
        return;
    }

    // Clamp values
    if (current_speed_ > max_speed_) current_speed_ = max_speed_;
    if (current_speed_ < min_speed_) current_speed_ = min_speed_;
    if (current_steering_angle_ > max_steering_angle_) current_steering_angle_ = max_steering_angle_;
    if (current_steering_angle_ < min_steering_angle_) current_steering_angle_ = min_steering_angle_;

    CommandRequest cmd;
    cmd.speed = current_speed_;
    cmd.steering_angle = current_steering_angle_;
    {
        std::unique_lock<std::mutex> lock(*cq_mu_);
        cq_->push(cmd);
    }
    cq_cv_->notify_one();
}
