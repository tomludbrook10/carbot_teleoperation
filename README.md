# carbot_teleoperation

Remote-control + data-recording rig for **carbot**, my self-driving RC car. I use this to
drive the car around by hand (keyboard or PS4 pad) and record everything — which is how I
collected the training data.

A gRPC link streams drive commands to the car and kinematics back, while GStreamer streams
live H.264 video from the Jetson camera. The recorder tees that camera feed to both the
live stream and an mp4, so every run gets saved as a rollout (video + control log) ready
for training.

**Stack:** C++ · gRPC · GStreamer · PS4 controller

## Part of the carbot project

- [carbot_drivetrain](https://github.com/tomludbrook10/carbot_drivetrain) — ESP32 drivetrain firmware
- [carbot_ws](https://github.com/tomludbrook10/carbot_ws) — the ROS2 brain that runs the live driving loop
- [carbot_action_model](https://github.com/tomludbrook10/carbot_action_model) — trains the image→action model
- [carbot_inference](https://github.com/tomludbrook10/carbot_inference) — real-time camera→waypoints model (TensorRT)
