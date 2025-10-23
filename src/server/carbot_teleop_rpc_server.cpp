#include "carbot_teleop_rpc_server.h"
#include "teleop_reactor.h"
#include <iostream>

#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/security/server_credentials.h>

CarbotTeleopServiceImpl::CarbotTeleopServiceImpl(std::queue<CommandRequest>* cq, std::mutex* cq_mu, std::condition_variable* cq_cv,
                                                 Kinematics* kinematics, std::mutex* k_mu, std::condition_variable* k_cv,
                                                 std::atomic<bool>* running, bool* k_updated)
    : cq_(cq), cq_mu_(cq_mu), cq_cv_(cq_cv),
      kinematics_(kinematics), k_mu_(k_mu), k_cv_(k_cv), running_(running), k_updated_(k_updated) {}

grpc::ServerUnaryReactor* CarbotTeleopServiceImpl::GetStatus(grpc::CallbackServerContext* context,
                                                             const google::protobuf::Empty* request,
                                                             carbot_teleop::StatusResponse* response) {
    response->set_status("Carbot is operational");
    auto* reactor = context->DefaultReactor();
    reactor->Finish(grpc::Status::OK);
    return reactor;
}

grpc::ServerBidiReactor<carbot_teleop::CommandRequest, carbot_teleop::CurrentKinematics>*
CarbotTeleopServiceImpl::StreamTeleoperation(grpc::CallbackServerContext* context) {
    return new TeleopReactor(cq_, cq_mu_, cq_cv_, kinematics_, k_mu_, k_cv_, running_, k_updated_);
}
