#pragma once

#include <gst/gst.h>
#include <string>
#include <mutex>
#include <thread>

#define ZERO_LATENCY 0x00000004
#define SUPERFAST 2

class ServerStreamer {
public:
    explicit ServerStreamer(const std::string client_address, const int client_port, const std::string rollout_directory = "./");
    ~ServerStreamer();

    bool setup();
    void run_async();
    void kill_run();

    uint64_t get_base_time() const { return base_time_; }

private:
    void run();

    GstElement *pipeline_;
    GstElement *camera_, *convert_, *encoder_, *h264parse_, *payloader_, *udpsink_;
    GstElement *camera_caps_filter_, *convert_caps_filter_;
    GstCaps *camera_caps_, *convert_caps_;

    // splitting elements. 
    GstElement *tee_, *streaming_queue_, *recording_queue_;

    // recording elements
    GstElement *mp4mux_, *file_sink_;
    GstBus *bus_;

    uint64_t base_time_;

    const std::string rollout_directory_;

    std::mutex pipeline_mu_;
    const std::string client_address_;
    const int client_port_;
    std::thread runner_;
};
