#!/bin/bash

# here we added the rtpjitterbuffer to handle network jitter and packet loss
# latency will 400 ms, will hold frame for 400ms till display, if comes after it will drop. 
# post-drop message will log the dropped packets info to console.

gst-launch-1.0 -v \
  udpsrc port=5000 \
    caps="application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264, payload=(int)96" \
  ! rtpjitterbuffer latency=50 drop-on-latency=true post-drop-messages=true \
  ! rtph264depay \
  ! h264parse \
  ! avdec_h264 \
  ! videoconvert \
  ! autovideosink sync=false