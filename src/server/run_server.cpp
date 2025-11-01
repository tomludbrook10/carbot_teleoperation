#include "teleoperation_server.h"
#include "defs.h"
#include <csignal>
#include <iostream>
#include <thread>
#include <atomic>

std::atomic<bool> running{true};

int main() {
    TeleoperationServer server("192.168.99.201", "192.168.99.254");

    std::signal(SIGINT, [](int){ 
        running.store(false, std::memory_order_relaxed);
        std::cout << "SIGINT received, shutting down server..." << std::endl;
    });

    std::thread server_thread([&server]() {
        CommandRequest command;
        while (server.IsRunning()) {
            if (server.GetNextCommand(command)) {
                command.Print();
            } else {
                break;
            }
        }
    });

    while (running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "Stopping server..." << std::endl;
    server.Stop();
    if (server_thread.joinable()) {
        server_thread.join();
        std::cout << "Server thread joined successfully." << std::endl;
    }

    return 0;
}
