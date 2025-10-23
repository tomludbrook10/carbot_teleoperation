#include "teleoperation_client.h"
#include "carbot_teleop_rpc_client.h"
#include "keyboard_controller.h"
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <iostream>

TeleoperationClient::TeleoperationClient(const std::string& server_address,
                                         const std::string& client_address,
                                         const int video_port,
                                         const int rpc_port)
    : server_address_(server_address),
      client_address_(client_address),
      video_port_(video_port) {
    rpc_thread_ = std::thread(&TeleoperationClient::RunRPCClient, this, server_address_, rpc_port);
}

TeleoperationClient::~TeleoperationClient() {
    stop_rpc_.store(true, std::memory_order_relaxed);
    cq_cv_.notify_one();
    if (rpc_thread_.joinable()) {
        rpc_thread_.join();
    }
}

void TeleoperationClient::Start() {
    KeyboardController controller(&cq_, &cq_mu_, &cq_cv_,
                                  &k_mu_, &current_kinematics_,
                                  &k_cv_, &k_updated_);
    controller.Run();
}

void TeleoperationClient::RunRPCClient(std::string server_address, int rpc_port) {
    CarbotTeleopRPCClient client(grpc::CreateChannel(
                    server_address + ":" + std::to_string(rpc_port),
                    grpc::InsecureChannelCredentials()),
                    &cq_, &cq_mu_, &cq_cv_,
                    &k_mu_, &current_kinematics_,
                    &k_cv_, &k_updated_, &stop_rpc_);

    std::string reply = client.GetStatus();
    std::cout << "Status: " << reply << std::endl;
    client.StreamTeleoperation();
}
