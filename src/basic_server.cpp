#include <grpc/grpc.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_context.h>

#include <string>

#include "carbot_teleop.grpc.pb.h"

class CarbotTeleopServiceImpl final : public carbot_teleop::CarbotTeleop::Service {
public:
  grpc::Status Start(grpc::ServerContext* context, const carbot_teleop::StartRequest* request,
                    carbot_teleop::StartResponse* response) override {
    std::string command = request->command();
    response->set_status("Started with command: " + command);
    return grpc::Status::OK;
  }
};

void RunServer() {
    std::string server_address("localhost:50051");
    CarbotTeleopServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
    return 0;
}