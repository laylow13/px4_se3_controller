#ifndef _STRUCTS_H
#define _STRUCTS_H

#include "Eigen/Eigen"

struct UAV_motion_linear_t {
    Eigen::Vector3d pos;
    Eigen::Vector3d vel;
    Eigen::Vector3d acc;
    Eigen::Vector3d jerk;
};

struct UAV_motion_angular_t {
    Eigen::Quaterniond q;
    Eigen::Vector3d vel;
    Eigen::Vector3d acc;
};

struct UAV_motion_t {
    UAV_motion_linear_t linear;
    UAV_motion_angular_t angular;
};

struct state_t {
    Eigen::Vector3d x;
    Eigen::Vector3d x_dot;
    Eigen::Vector3d x_2dot;
    Eigen::Vector3d x_3dot;
    Eigen::Vector3d x_4dot;
    Eigen::Quaterniond q;
    Eigen::Vector3d w;
    Eigen::Vector3d w_dot;
};
struct command_t {
    Eigen::Vector3d x;
    Eigen::Vector3d x_dot;
    Eigen::Vector3d x_2dot;
    Eigen::Vector3d x_3dot;
    Eigen::Vector3d x_4dot;
    Eigen::Vector3d b1d;
    Eigen::Vector3d b1d_dot;
};

#endif