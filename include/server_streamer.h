#pragma once

#include <gst/gst.h>
#include <string>
#include <mutex>
#include <thread>

class ServerStreamer {
public:
    explicit ServerStreamer(const std::string client_address, const int client_port);
    ~ServerStreamer();

    bool setup();
    void run_async();
    void kill_run();

private:
    void run();

    GstElement *pipeline_;
    GstBus *bus_;
    std::mutex pipeline_mu_;
    const std::string client_address_;
    const int client_port_;
    std::thread runner_;
};
