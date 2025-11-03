#pragma once

#include <gst/gst.h>
#include <inttypes.h>


G_BEGIN_DECLS


#define GST_TYPE_TIMESTAMP_LOGGER (gst_timestamp_logger_get_type())
#define GST_TIMESTAMP_LOGGER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIMESTAMP_LOGGER,GstTimestampLogger))
#define GST_IS_TIMESTAMP_LOGGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIMESTAMP_LOGGER))
#define GST_TIMESTAMP_LOGGER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIMESTAMP_LOGGER,GstTimestampLoggerClass))
#define GST_IS_TIMESTAMP_LOGGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_TIMESTAMP_LOGGER))

typedef struct _GstTimestampLogger {
    GstElement element;
    GstPad *sinkpad;
    GstPad *srcpad;
    gchar *file_path;
    gboolean first_buffer_received;
    uint64_t first_timestamp;
} GstTimestampLogger;

typedef struct _GstTimestampLoggerClass {
    GstElementClass parent_class;
} GstTimestampLoggerClass;

GType gst_timestamp_logger_get_type(void);
G_END_DECLS