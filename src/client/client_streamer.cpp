#include "client_streamer.h"
#include <gst/gst.h>
#include <sstream>
#include <iostream>

ClientStreamer::ClientStreamer(const int client_port)
    : pipeline_(nullptr), bus_(nullptr), client_port_(client_port) {}

ClientStreamer::~ClientStreamer() {
    // if kill_run is already, been called it does nothing.
    //kill_run();
    // clean up
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
    }
    if (bus_)
        gst_object_unref(bus_);
}

bool ClientStreamer::setup() {

    gst_init(nullptr, nullptr);

    std::ostringstream pipeline_stream;
    pipeline_stream << "udpsrc port="
                    << std::to_string(client_port_)
                    << "! "
                    << "rtph264depay !"
                    << "h264parse !"
                    << "avdec_h264 ! "
                    << "videoconvert ! "
                    << "autovideosink sync=false";


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

void ClientStreamer::run_async() {
    runner_ = std::thread(&ClientStreamer::run, this);
}

void ClientStreamer::kill_run() {
    if (pipeline_) {
        std::lock_guard<std::mutex> lock(pipeline_mu_);
        gst_element_send_event(pipeline_, gst_event_new_eos());
    }

    if (runner_.joinable())
        runner_.join();
}

void ClientStreamer::run() {
    //{
        //std::lock_guard<std::mutex> lock(pipeline_mu_);
        if (bus_ == nullptr || pipeline_  == nullptr) {
            std::cerr << "Error: must set up pipeline and bus first" << std::endl;
            return;
        }
        
     //   gst_macos_main(); 
        gst_element_set_state(pipeline_, GST_STATE_PLAYING);
        g_print("Pipeline is running...\n");
   // }

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

int main() {
    ClientStreamer streamer(5000);
    if (!streamer.setup()) {
        std::cerr << "Failed to set up client streamer" << std::endl;
        return -1;
    }
    streamer.run();
    //streamer.kill_run();
    return 0;
}
