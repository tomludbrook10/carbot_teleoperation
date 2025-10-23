#include "carbot_teleop_rpc_client.h"
#include "teleop_client_reactor.h"
#include <grpcpp/client_context.h>
#include <iostream>
#include <thread>
#include <chrono>

CarbotTeleopRPCClient::CarbotTeleopRPCClient(std::shared_ptr<grpc::Channel> channel,
                                             std::queue<CommandRequest>* cq,
                                             std::mutex* cq_mu,
                                             std::condition_variable* cq_cv,
                                             std::mutex* k_mu,
                                             Kinematics* current_kinematics,
                                             std::condition_variable* k_cv,
                                             bool* k_updated,
                                             std::atomic<bool>* stop_rpc)
    : stub_(carbot_teleop::CarbotTeleop::NewStub(channel)),
      cq_(cq), cq_mu_(cq_mu), cq_cv_(cq_cv),
      k_mu_(k_mu), current_kinematics_(current_kinematics),
      k_cv_(k_cv), k_updated_(k_updated),
      stop_rpc_(stop_rpc) {}

std::string CarbotTeleopRPCClient::GetStatus() {
    google::protobuf::Empty request;
    carbot_teleop::StatusResponse response;
    grpc::ClientContext context;

    std::mutex mu;
    std::condition_variable cv;
    bool done = false;

    stub_->async()->GetStatus(&context, &request, &response,
        [&mu, &cv, &done](grpc::Status status) {
            if (!status.ok()) {
                std::cerr << "RPC failed: " << status.error_code() << ": " << status.error_message() << std::endl;
            }
            std::lock_guard<std::mutex> lock(mu);
            done = true;
            cv.notify_one();
        });
    {
        std::unique_lock<std::mutex> lock(mu);
        cv.wait(lock, [&done] { return done; });
    }
    return response.status();
}

void CarbotTeleopRPCClient::StreamTeleoperation() {
    TeleopClientReactor reactor(stub_.get(), cq_, cq_mu_, cq_cv_, k_mu_, current_kinematics_, k_cv_, k_updated_);

    std::thread stopper([this, &reactor]() {
        while (!stop_rpc_->load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        reactor.Stop();
    });

    grpc::Status status = reactor.Await();
    stopper.join();

    if (!status.ok()) {
        std::cerr << "StreamTeleoperation RPC failed: " << status.error_code() << ": " << status.error_message() << std::endl;
    } else {
        std::cout << "StreamTeleoperation RPC completed successfully." << std::endl;
    }
}
