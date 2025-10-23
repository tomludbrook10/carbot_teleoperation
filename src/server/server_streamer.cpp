#include "server_streamer.h"
#include <gst/gst.h>
#include <sstream>
#include <iostream>

ServerStreamer::ServerStreamer(const std::string client_address, const int client_port)
    : pipeline_(nullptr), bus_(nullptr), client_address_(client_address), client_port_(client_port) {}

ServerStreamer::~ServerStreamer() {
    // if kill_run is already, been called it does nothing.
    kill_run();
    // clean up
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
    }
    if (bus_)
        gst_object_unref(bus_);
}

bool ServerStreamer::setup() {

    gst_init(nullptr, nullptr);

    std::ostringstream pipeline_stream;
    pipeline_stream << "nvarguscamerasrc sensor-id=0 ! "
                    << "video/x-raw(memory:NVMM), width=1920, height=1080, framerate=30/1, format=NV12 ! "
                    << "nvvidconv ! "
                    << "video/x-raw, format=NV12 ! "
                    << "x264enc bitrate=4000 speed-preset=superfast tune=zerolatency ! "
                    << "h264parse config-interval=1 ! "
                    << "rtph264pay pt=96 ! "
                    << "udpsink host="
                    << client_address_
                    << " port="
                    << std::to_string(client_port_)
                    << " sync=false";

    std::lock_guard<std::mutex> lock(pipeline_mu_);
    pipeline_ = gst_parse_launch(pipeline_stream.str().c_str(), nullptr);
    if (!pipeline_) {
        g_printerr ("Failed to create pipeline from description\n");
        return false;
    }

    bus_ = gst_element_get_bus(pipeline_);
    if (!bus_) {
        g_printerr("Failed to create bus");
        return false;
    }
    std::cout << "Successfully set up pipeline and bus" << std::endl;
    return true;
}

void ServerStreamer::run_async() {
    runner_ = std::thread(&ServerStreamer::run, this);
}

void ServerStreamer::kill_run() {
    if (pipeline_) {
        std::lock_guard<std::mutex> lock(pipeline_mu_);
        std::cout << "Sending EOS event to pipeline" << std::endl;
        gst_element_send_event(pipeline_, gst_event_new_eos());
    }

    if (runner_.joinable())
        runner_.join();
}

void ServerStreamer::run() {
    {
        std::lock_guard<std::mutex> lock(pipeline_mu_);
        if (bus_ == nullptr || pipeline_  == nullptr) {
            std::cerr << "Error: must set up pipeline and bus first" << std::endl;
            return;
        }

        gst_element_set_state(pipeline_, GST_STATE_PLAYING);
        g_print("Pipeline is running...\n");
    }

    GstMessage *msg = gst_bus_timed_pop_filtered(bus_, GST_CLOCK_TIME_NONE,
                                            static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    if (msg != NULL) {
        GError *err;
        gchar *dbg;
        switch (GST_MESSAGE_TYPE (msg)) {
            case GST_MESSAGE_ERROR:
                gst_message_parse_error (msg, &err, &dbg);
                g_printerr ("Error received from element %s: %s\n",
                            GST_OBJECT_NAME (msg->src), err->message);
                g_error_free (err);
                g_free (dbg);
                break;
            case GST_MESSAGE_EOS:
                g_print ("End‐of‐Stream reached.\n");
                break;
            default:
                /* Unexpected */
                break;
        }
        gst_message_unref (msg);
    }
    g_print("Finished streaming");
}
