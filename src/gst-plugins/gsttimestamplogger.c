#include <gst/gst.h>
#include "gsttimestamplogger.h"
#include <inttypes.h>
#include <stdio.h>
#include <errno.h>

// refer to: https://gstreamer.freedesktop.org/documentation/plugin-development/basics/boiler.html?gi-language=c

G_DEFINE_TYPE(GstTimestampLogger, gst_timestamp_logger, GST_TYPE_ELEMENT);

enum {
    PROP_0, 
    PROP_FILE_PATH
};

static GstStaticPadTemplate sink_factory =
  GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
      "video/x-raw(memory:NVMM), format=(string)NV12, width=(int)[1,2147483647], height=(int)[1,2147483647], framerate=(fraction)[0/1,2147483647/1]"
    )
  );
static GstStaticPadTemplate src_factory =
  GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
      "video/x-raw(memory:NVMM), format=(string)NV12, width=(int)[1,2147483647], height=(int)[1,2147483647], framerate=(fraction)[0/1,2147483647/1]"
    )
  );

static GParamSpec *properties[PROP_FILE_PATH + 1] = { NULL, };

static GstFlowReturn gst_timestamp_logger_chain(GstPad *pad, GstObject *parent, GstBuffer *buf);
static void gst_timestamp_logger_set_property(GObject *object, guint prop_id,
                                        const GValue *value, GParamSpec *pspec);
static void gst_timestamp_logger_get_property(GObject *object, guint prop_id,
                                        GValue *value, GParamSpec *pspec);
static gboolean gst_timestamp_logger_event(GstPad *pad, GstObject *parent, GstEvent *event);
static void gst_timestamp_logger_finalize(GObject *object);
                                    

// register the element details. 
// init the class only once, specifiying what singals, arguments has and setting up global state. 
static void gst_timestamp_logger_class_init(GstTimestampLoggerClass *klass) {
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gst_element_class_set_static_metadata(element_class,
        "Timestamp Logger",
        "Filter/Logger",
        "Logs the timestamps of incoming buffers",
        "Your Name <my.email@example.com>");

    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get(&sink_factory));
    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get(&src_factory));

    // properties
    gobject_class->set_property = gst_timestamp_logger_set_property;
    gobject_class->get_property = gst_timestamp_logger_get_property;
    gobject_class->finalize = gst_timestamp_logger_finalize;

    properties[PROP_FILE_PATH] = g_param_spec_string(
        "file-path",
        "File Path",
        "Path to the log file",
        NULL,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
    );
    g_object_class_install_properties(gobject_class, PROP_FILE_PATH + 1, properties);
}

// init each instance of the element.
static void gst_timestamp_logger_init(GstTimestampLogger *logger) {
    // Instance initialization code here (if needed)
    logger->sinkpad = gst_pad_new_from_static_template(&sink_factory, "sink");

    // chain function.
    gst_pad_set_chain_function(logger->sinkpad, gst_timestamp_logger_chain);
    gst_element_add_pad(GST_ELEMENT(logger), logger->sinkpad);

    // these two lines where need to ensure I'm passing cap through
    GST_PAD_SET_PROXY_CAPS(logger->sinkpad);


    logger->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
    gst_element_add_pad(GST_ELEMENT(logger), logger->srcpad);

    // these two lines where need to ensure I'm passing cap through
    GST_PAD_SET_PROXY_CAPS(logger->srcpad);

    logger->first_buffer_received = FALSE;
    logger->first_timestamp = 0;
}

static gboolean gst_timestamp_logger_event(GstPad *pad, GstObject *parent, GstEvent *event) {
    // Handle events if necessary

    GstTimestampLogger *logger = GST_TIMESTAMP_LOGGER(parent);
    gboolean ret = gst_pad_event_default(pad, parent, event);
    return ret;
}


static GstFlowReturn gst_timestamp_logger_chain(GstPad *pad, GstObject *parent, GstBuffer *buf) { 
    GstTimestampLogger *logger = GST_TIMESTAMP_LOGGER(parent);

    if (!logger->first_buffer_received) {
        logger->first_buffer_received = TRUE;
        GstClockTime timestamp = GST_BUFFER_PTS(buf);
        uint64_t time_nano = (uint64_t)timestamp;
        logger->first_timestamp = time_nano;
    }
    return gst_pad_push(logger->srcpad, buf);
}

static void gst_timestamp_logger_set_property(GObject *object, guint prop_id, 
                                        const GValue *value, GParamSpec *pspec) {
    GstTimestampLogger *logger = GST_TIMESTAMP_LOGGER(object);

    switch (prop_id) {
        case PROP_FILE_PATH:
            g_free(logger->file_path);
            logger->file_path = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}


static void gst_timestamp_logger_get_property(GObject *object, guint prop_id,
                                        GValue *value, GParamSpec *pspec) {
    GstTimestampLogger *logger = GST_TIMESTAMP_LOGGER(object);

    switch (prop_id) {
        case PROP_FILE_PATH:
            g_value_set_string(value, logger->file_path);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

int write_timestamp_to_file(const char* file_path, uint64_t timestamp) {
    if (file_path == NULL || file_path[0] == '\0') {
        return EINVAL;
    }

    FILE* file = fopen(file_path, "wb");
    if (file == NULL) {
        return errno;
    }

    fprintf(file, "%" PRIu64 "\n", timestamp);
    fclose(file);
    return 0;
}

static void gst_timestamp_logger_finalize(GObject *object) {
    GstTimestampLogger *logger = GST_TIMESTAMP_LOGGER(object);

    printf("closing timestamplogger\n");
    write_timestamp_to_file(logger->file_path, logger->first_timestamp);
    g_free(logger->file_path);
    G_OBJECT_CLASS(gst_timestamp_logger_parent_class)->finalize(object);
}


// called as soon as the plugin is loaded.
// return true or false, depending on whether the plugin was initialized successfully.
static gboolean plugin_init(GstPlugin *plugin) {
    return gst_element_register(plugin, "timestamplogger", GST_RANK_NONE, GST_TYPE_TIMESTAMP_LOGGER);
}

#ifndef PACKAGE
#define PACKAGE "timestamplogger"
#endif


GST_PLUGIN_DEFINE(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    timestamplogger,
    "Timestamp Logger Plugin",
    plugin_init,
    "1.0",
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)