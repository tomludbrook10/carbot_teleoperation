#!/bin/bash
export GST_PLUGIN_PATH=/home/tom/carbot_teleoperation/src/gst-plugins
gcc -fPIC -shared -o $GST_PLUGIN_PATH/libgsttimestamplogger.so $GST_PLUGIN_PATH/gsttimestamplogger.c $(pkg-config --cflags --libs gstreamer-1.0)