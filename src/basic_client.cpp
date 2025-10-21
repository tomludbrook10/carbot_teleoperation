#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "carbot_teleop.grpc.pb.h"

#include <iostream>
#include <string>

class CarbotTeleopClient {
public:
    CarbotTeleopClient(std::shared_ptr<grpc::Channel> channel)
        : stub_(carbot_teleop::CarbotTeleop::NewStub(channel)) {}
    
    std::string Start(const std::string& command) {
        carbot_teleop::StartRequest request;
        request.set_command(command);
    
        carbot_teleop::StartResponse response;
        grpc::ClientContext context;

        grpc::Status status = stub_->Start(&context, request, &response);

        if (status.ok()) {
        return response.status();
        } else {
            return "RPC failed: " + std::to_string(status.error_code()) + ": " + status.error_message();
        }
    }
    
private:
    std::unique_ptr<carbot_teleop::CarbotTeleop::Stub> stub_;
};

int main(int argc, char** argv) {
    CarbotTeleopClient client(grpc::CreateChannel(
        "localhost:50051", grpc::InsecureChannelCredentials()));
    
    std::string command = "Start Teleoperation now";
    std::string reply = client.Start(command);
    std::cout << "Response from server: " << reply << std::endl;
    
    return 0;
}