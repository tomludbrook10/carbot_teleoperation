#pragma once

#include <gst/gst.h>
#include <mutex>
#include <thread>

class ClientStreamer {
public:
    explicit ClientStreamer(const int client_port);
    ~ClientStreamer();

    bool setup();
    void run_async();
    void kill_run();
    void run();
    
private:
    GstElement *pipeline_;
    GstBus *bus_;
    std::mutex pipeline_mu_;
    const int client_port_;
    std::thread runner_;
};
