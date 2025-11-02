#include <gst/gst.h>
#include <iostream>
#include <chrono>
#include <thread>

#define CLIENT_ADDRESS "192.168.99.129"
#define CLIENT_PORT 5000
#define ZERO_LATENCY 0x00000004
#define SUPERFAST 2

int run() {
    GstElement *pipeline;
    GstElement *camera, *convert, *encoder, *h264parse, *payloader, *udpsink;
    GstElement *camera_caps_filter, *convert_caps_filter;
    GstCaps *camera_caps, *convert_caps;

    // splitting elements. 
    GstElement *tee, *streaming_queue, *recording_queue;

    // recording elements
    GstElement *mp4mux, *file_sink;


    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;
    
    gst_init (nullptr, nullptr);
    camera = gst_element_factory_make ("nvarguscamerasrc", "camera");
    convert = gst_element_factory_make ("nvvidconv", "convert");
    encoder = gst_element_factory_make ("x264enc", "encoder");
    h264parse = gst_element_factory_make ("h264parse", "h264parse");
    payloader = gst_element_factory_make ("rtph264pay", "payloader");
    udpsink = gst_element_factory_make ("udpsink", "udpsink");
    camera_caps_filter = gst_element_factory_make ("capsfilter", "camera_caps_filter");
    convert_caps_filter = gst_element_factory_make ("capsfilter", "convert_caps_filter");
    tee = gst_element_factory_make ("tee", "tee");
    streaming_queue = gst_element_factory_make ("queue", "streaming_queue");
    recording_queue = gst_element_factory_make ("queue", "recording_queue");
    mp4mux = gst_element_factory_make ("mp4mux", "mp4mux");
    file_sink = gst_element_factory_make ("filesink", "file_sink");

    if (!camera || 
        !convert || 
        !encoder || 
        !h264parse || 
        !payloader || 
        !udpsink || 
        !tee || 
        !streaming_queue || 
        !recording_queue || 
        !mp4mux || 
        !file_sink) {
        g_printerr ("Not all elements could be created.\n");
        return -1;
    }

    // set caps. 
    camera_caps = gst_caps_from_string(
        "video/x-raw(memory:NVMM), width=1920, height=1080, framerate=30/1, format=NV12");
    convert_caps = gst_caps_from_string(
        "video/x-raw, format=NV12");

    g_object_set (G_OBJECT (camera_caps_filter), "caps", camera_caps, nullptr);
    g_object_set (G_OBJECT (convert_caps_filter), "caps", convert_caps, nullptr);

    // set elelment properties
    g_object_set (G_OBJECT (camera), "sensor-id", 0, nullptr);
    g_object_set (G_OBJECT (encoder), 
        "bitrate", 4000,
        "speed-preset", SUPERFAST, // superfast
        "tune", ZERO_LATENCY, // zerolatency
        "key-int-max", 30,
        nullptr);
    g_object_set (G_OBJECT (h264parse), "config-interval", 4, nullptr);
    g_object_set (G_OBJECT (payloader), "pt", 96, nullptr);
    g_object_set (G_OBJECT (udpsink), 
        "host", CLIENT_ADDRESS,
        "port", CLIENT_PORT,
        "sync", false,
        nullptr);

    g_object_set (G_OBJECT (file_sink), 
        "location", "recording.mp4",
        nullptr);

    pipeline = gst_pipeline_new ("pipeline");

    // the bin is a container around an element
    gst_bin_add_many (GST_BIN (pipeline),
                      camera, 
                      camera_caps_filter,
                      convert,
                      convert_caps_filter,
                      encoder,
                      h264parse,
                      nullptr);

    gst_bin_add_many (GST_BIN (pipeline),
                    tee,
                    streaming_queue,
                    payloader,
                    udpsink,
                    recording_queue,
                    mp4mux,
                    file_sink,
                    nullptr);

    if (gst_element_link (camera, camera_caps_filter) != TRUE || 
        gst_element_link (camera_caps_filter, convert) != TRUE ||
        gst_element_link (convert, convert_caps_filter) != TRUE ||
        gst_element_link (convert_caps_filter, encoder) != TRUE ||
        gst_element_link (encoder, h264parse) != TRUE ||
        gst_element_link (h264parse, tee) != TRUE) {
        g_printerr ("Elements could not be linked.\n");
        gst_object_unref (pipeline);
        return -1;
    }

    if (gst_element_link_many (tee, streaming_queue, payloader, udpsink, nullptr) != TRUE ||
        gst_element_link_many (tee, recording_queue, mp4mux, file_sink, nullptr) != TRUE) {
        g_printerr ("Tee elements could not be linked.\n");
        gst_object_unref (pipeline);
        return -1;
    }

    GstClock *clock = gst_system_clock_obtain();
    g_object_set (clock, "clock-type", GST_CLOCK_TYPE_MONOTONIC, nullptr);
    gst_pipeline_use_clock (GST_PIPELINE (pipeline), clock);
    gst_object_unref (clock);

    ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr ("Unable to set the pipeline to the playing state.\n");
        gst_object_unref (pipeline);
        return -1;
    }

    GstClockTime base_time;
    base_time = gst_element_get_base_time (pipeline);

    std::cout << "base time " << base_time << std::endl;    

    g_print ("Base time of camera element: %" GST_TIME_FORMAT "\n",
    GST_TIME_ARGS (base_time));

    bus = gst_element_get_bus (pipeline);

    GstClockTime timeout = 10 * GST_SECOND;

    msg = gst_bus_timed_pop_filtered (bus, timeout,
        static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));


    gst_element_send_event(pipeline, gst_event_new_eos());
    std::this_thread::sleep_for(std::chrono::seconds(2));

      /* Parse message */
    if (msg != NULL) {
        GError *err;
        gchar *debug_info;

        switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR:
            gst_message_parse_error (msg, &err, &debug_info);
            g_printerr ("Error received from element %s: %s\n",
                GST_OBJECT_NAME (msg->src), err->message);
            g_printerr ("Debugging information: %s\n",
                debug_info ? debug_info : "none");
            g_clear_error (&err);
            g_free (debug_info);
            break;
        case GST_MESSAGE_EOS:
            g_print ("End-Of-Stream reached.\n");
            break;
        default:
            /* We should not reach here because we only asked for ERRORs and EOS */
            g_printerr ("Unexpected message received.\n");
            break;
        }
        gst_message_unref (msg);
    }

    std::cout << "Stopping recording and streaming..." << std::endl;

    /* Free resources */
    gst_object_unref (bus);
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline);
    return 0;
}

int main () {

    return run();
}
