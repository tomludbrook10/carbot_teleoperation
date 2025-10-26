#pragma once

#include <string>
#include <iostream>

/*
Mimics the gRPC defintions such that they remain ecapsulated in the rpc code. 
*/

struct CommandRequest {
  float speed;
  float steering_angle;

  void Print() {
    std::cout << "CommandRequest: " << std::endl;
    std::cout << "  Speed: " << speed << std::endl;
    std::cout << "  Steering Angle: " << steering_angle << std::endl;
  }
};

struct Point {
  float x;
  float y;

  bool Equals(const Point& other) {
    return other.x == x && other.y == y;
  }
};

struct Pose {
  Point position;
  float orientation;

  bool Equals(const Pose& other) {
    return position.Equals(other.position) && orientation == other.orientation;
  }
};

struct Kinematics {
    float current_speed;
    float current_steering_angle;
    Pose pose;

    Kinematics() : current_speed(0.0f), current_steering_angle(0.0f) {}

    Kinematics(float current_speed, float current_steering_angle, float orientation, float x, float y)
        : current_speed(current_speed), current_steering_angle(current_steering_angle) {
            pose.orientation = orientation;
            pose.position.x = x;
            pose.position.y = y;
        }

    bool Equals(const Kinematics& other) {
        return pose.Equals(other.pose) &&  
        current_speed == other.current_speed && 
        current_steering_angle == other.current_steering_angle;
    }

    void Print() {
        std::cout << "Kinematics: " << std::endl;
        std::cout << "  Speed: " << current_speed << std::endl;
        std::cout << "  Steering Angle: " << current_steering_angle << std::endl;
        std::cout << "  Position: (" << pose.position.x << ", " << pose.position.y << ")" << std::endl;
        std::cout << "  Orientation: " << pose.orientation << std::endl;
    }
};

struct StatusResponse {
    std::string status;
};
