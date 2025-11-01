#include <gst/gst.h>


#define CLIENT_ADDRESS "192.168.99.254"
#define CLIENT_PORT 5000
#define ZERO_LATENCY 0x00000004
#define SUPERFAST 2


int run() {
    GstElement *pipeline;
    GstElement *camera, *convert, *encoder, *h256parse, *payloader, *udpsink;
    GstElement *camera_caps_filter, *convert_caps_filter;
    GstCaps *camera_caps, *convert_caps;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;
    
    gst_init (nullptr, nullptr);
    camera = gst_element_factory_make ("nvarguscamerasrc", "camera");
    convert = gst_element_factory_make ("nvvidconv", "convert");
    encoder = gst_element_factory_make ("x264enc", "encoder");
    h256parse = gst_element_factory_make ("h264parse", "h256parse");
    payloader = gst_element_factory_make ("rtph264pay", "payloader");
    udpsink = gst_element_factory_make ("udpsink", "udpsink");
    camera_caps_filter = gst_element_factory_make ("capsfilter", "camera_caps_filter");
    convert_caps_filter = gst_element_factory_make ("capsfilter", "convert_caps_filter");

    if (!pipeline || !camera || !convert || !encoder || !h256parse || !payloader || !udpsink) {
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
    g_object_set (G_OBJECT (h256parse), "config-interval", 4, nullptr);
    g_object_set (G_OBJECT (payloader), "pt", 96, nullptr);
    g_object_set (G_OBJECT (udpsink), 
        "host", CLIENT_ADDRESS,
        "port", CLIENT_PORT,
        "sync", false,
        nullptr);

    pipeline = gst_pipeline_new ("pipeline");


    // the bin is a container around an element
    gst_bin_add_many (GST_BIN (pipeline),
                      camera, 
                      convert, 
                      encoder, 
                      h256parse, 
                      payloader, 
                      udpsink, 
                      nullptr);

    if (gst_element_link (camera, convert) != TRUE || 
        gst_element_link (convert, encoder) != TRUE ||
        gst_element_link (encoder, h256parse) != TRUE || 
        gst_element_link (h256parse, payloader) != TRUE ||
        gst_element_link (payloader, udpsink) != TRUE) {
        g_printerr ("Elements could not be linked.\n");
        gst_object_unref (pipeline);
        return -1;
    }

    ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr ("Unable to set the pipeline to the playing state.\n");
        gst_object_unref (pipeline);
        return -1;
    }

    bus = gst_element_get_bus (pipeline);
    msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
        static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

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

    /* Free resources */
    gst_object_unref (bus);
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline);
    gst_object_unref (camera_caps);
    gst_object_unref (convert_caps);
    gst_object_unref (encoder_caps);
    gst_object_unref (h256parse_caps);
    gst_object_unref (payloader_caps);
    gst_object_unref (udpsink_caps);
    return 0;
}

int main () {

    return run();
}