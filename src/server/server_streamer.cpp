#include "server_streamer.h"
#include <gst/gst.h>
#include <sstream>
#include <iostream>
#include <fstream>

ServerStreamer::ServerStreamer(const std::string client_address, const int client_port, const std::string rollout_directory)
    : pipeline_(nullptr), bus_(nullptr), client_address_(client_address), client_port_(client_port), rollout_directory_(rollout_directory) {}

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

    camera_ = gst_element_factory_make ("nvarguscamerasrc", "camera");
    convert_ = gst_element_factory_make ("nvvidconv", "convert");
    encoder_ = gst_element_factory_make ("x264enc", "encoder");
    h264parse_ = gst_element_factory_make ("h264parse", "h264parse");
    payloader_ = gst_element_factory_make ("rtph264pay", "payloader");
    udpsink_ = gst_element_factory_make ("udpsink", "udpsink");
    camera_caps_filter_ = gst_element_factory_make ("capsfilter", "camera_caps_filter");
    convert_caps_filter_ = gst_element_factory_make ("capsfilter", "convert_caps_filter");
    tee_ = gst_element_factory_make ("tee", "tee");
    streaming_queue_ = gst_element_factory_make ("queue", "streaming_queue");
    recording_queue_ = gst_element_factory_make ("queue", "recording_queue");
    mp4mux_ = gst_element_factory_make ("mp4mux", "mp4mux");
    file_sink_ = gst_element_factory_make ("filesink", "file_sink");
    timestamp_logger_ = gst_element_factory_make("timestamplogger", "timestamp_logger");

    if (!camera_ || 
        !convert_ || 
        !encoder_ || 
        !h264parse_ || 
        !payloader_ || 
        !udpsink_ || 
        !tee_ || 
        !streaming_queue_ || 
        !recording_queue_ || 
        !mp4mux_ || 
        !file_sink_ ||
        !timestamp_logger_ ||
        !camera_caps_filter_ ||
        !convert_caps_filter_) {
        g_printerr ("Not all elements could be created.\n");
        return false;
    }

    camera_caps_ = gst_caps_from_string(
        "video/x-raw(memory:NVMM), width=1920, height=1080, framerate=30/1, format=NV12");
    convert_caps_ = gst_caps_from_string(
        "video/x-raw, format=NV12");

    g_object_set (G_OBJECT (camera_caps_filter_), "caps", camera_caps_, nullptr);
    g_object_set (G_OBJECT (convert_caps_filter_), "caps", convert_caps_, nullptr);

    // set elelment properties
    g_object_set (G_OBJECT (camera_), "sensor-id", 0, nullptr);
    g_object_set (G_OBJECT (encoder_), 
        "bitrate", 4000,
        "speed-preset", SUPERFAST, // superfast
        "tune", ZERO_LATENCY, // zerolatency
        "key-int-max", 30,
        nullptr);

    g_object_set (G_OBJECT (h264parse_), "config-interval", 4, nullptr);
    g_object_set (G_OBJECT (payloader_), "pt", 96, nullptr);
    g_object_set (G_OBJECT (udpsink_), 
        "host", client_address_.c_str(),
        "port", client_port_,
        "sync", false,
        nullptr);
        
    std::string file_location = rollout_directory_ + "/recording.mp4";

    g_object_set (G_OBJECT (file_sink_), 
        "location", file_location.c_str(),
        nullptr);

    std::string timestamp_file = rollout_directory_ + "/timestamp_log.txt";
    g_object_set (G_OBJECT (timestamp_logger_),
        "file-path", timestamp_file.c_str(),
        nullptr);

    pipeline_ = gst_pipeline_new ("pipeline");

    // the bin is a container around an element
    gst_bin_add_many (GST_BIN (pipeline_),
                      camera_, 
                      camera_caps_filter_,
                      convert_,
                      convert_caps_filter_,
                      encoder_,
                      timestamp_logger_,
                      h264parse_,
                      nullptr);

    gst_bin_add_many (GST_BIN (pipeline_),
                    tee_,
                    streaming_queue_,
                    payloader_,
                    udpsink_,
                    recording_queue_,
                    mp4mux_,
                    file_sink_,
                    nullptr);

    if (gst_element_link (camera_, camera_caps_filter_) != TRUE || 
        gst_element_link (camera_caps_filter_, timestamp_logger_) != TRUE ||
        gst_element_link (timestamp_logger_, convert_) != TRUE ||
        gst_element_link (convert_, convert_caps_filter_) != TRUE ||
        gst_element_link (convert_caps_filter_, encoder_) != TRUE ||
        gst_element_link (encoder_, h264parse_) != TRUE ||
        gst_element_link (h264parse_, tee_) != TRUE) {
        g_printerr ("Elements could not be linked.\n");
        return false;
    }

    if (gst_element_link_many (tee_, streaming_queue_, payloader_, udpsink_, nullptr) != TRUE ||
        gst_element_link_many (tee_, recording_queue_, mp4mux_, file_sink_, nullptr) != TRUE) {
        g_printerr ("Tee elements could not be linked.\n");
        gst_object_unref (pipeline_);
        return false;
    }
    bus_ = gst_element_get_bus (pipeline_);

    /// setting up clock. 
    GstClock *clock = gst_system_clock_obtain();
    g_object_set (clock, "clock-type", GST_CLOCK_TYPE_MONOTONIC, nullptr);
    gst_pipeline_use_clock (GST_PIPELINE (pipeline_), clock);
    gst_object_unref (clock);
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

        GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            g_printerr ("Unable to set the pipeline to the playing state.\n");
            gst_object_unref (pipeline_);
            return;
        }
    }

    base_time_ = static_cast<uint64_t>(gst_element_get_base_time (camera_));
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

    std::string file_location = rollout_directory_ + "/start_time.txt";
    std::ofstream outfile(file_location);
    outfile << base_time_ << std::endl;
    outfile.close();

    g_print("Finished streaming\n");
}
