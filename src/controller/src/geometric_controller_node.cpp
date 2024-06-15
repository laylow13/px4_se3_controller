//
// Created by lay on 24-5-15.
//
#include <chrono>
#include <memory>
#include <Eigen/Eigen>
#include <string>
#include "rclcpp/rclcpp.hpp"
#include "utils/msg/uav_command.hpp"
#include "utils/msg/uav_state_feedback.hpp"
#include "px4_msgs/msg/offboard_control_mode.hpp"
#include "px4_msgs/msg/vehicle_status.hpp"
#include "px4_msgs/msg/vehicle_thrust_setpoint.hpp"
#include "px4_msgs/msg/vehicle_torque_setpoint.hpp"
#include "geometric_control/Geometric_control.hpp"

using namespace std::chrono_literals;
using std::placeholders::_1;

class Controller_node : public rclcpp::Node {
public:
    Controller_node() : Node("geometric_controller_node"), is_posctl(false) {
        state = std::make_shared<state_t>();
        command = std::make_shared<command_t>();
        geometric_param = std::make_shared<geometric_param_t>();
        controller = std::make_shared<Geometric_control>();
        controller->init(command, state, geometric_param);
        parameter_init();
        parameter_update();
        thrust_cmd_pub = this->create_publisher<px4_msgs::msg::VehicleThrustSetpoint>(
                "/fmu/in/vehicle_thrust_setpoint", 10);
        torque_cmd_pub = this->create_publisher<px4_msgs::msg::VehicleTorqueSetpoint>(
                "/fmu/in/vehicle_torque_setpoint", 10);
        offboard_pub = this->create_publisher<px4_msgs::msg::OffboardControlMode>("/fmu/in/offboard_control_mode", 10);
        state_sub = this->create_subscription<utils::msg::UAVStateFeedback>(
                "/SCIT_drone/UAV_state_feedback", 10, std::bind(&Controller_node::state_sub_cb, this, _1));
        command_sub = this->create_subscription<utils::msg::UAVCommand>(
                "/SCIT_drone/UAV_command", 10, std::bind(&Controller_node::command_sub_cb, this, _1));
        mode_sub = this->create_subscription<px4_msgs::msg::VehicleStatus>("/fmu/out/vehicle_status", 10,
                                                                           [this](const px4_msgs::msg::VehicleStatus::UniquePtr msg) {
                                                                               is_posctl = msg->nav_state == 2;
                                                                               is_offboard = msg->nav_state == 14;
                                                                           });
        timer_ = this->create_wall_timer(
                std::chrono::milliseconds(int(1000)),
                std::bind(&Controller_node::timer_callback, this));
    }

private:
    bool is_posctl;
    bool is_offboard;
    std::map<std::string, double> parameters_;
    std::atomic<uint64_t> timestamp;
    shared_ptr<Control_base> controller;
    shared_ptr<command_t> command;
    shared_ptr<state_t> state;
    shared_ptr<geometric_param_t> geometric_param;
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Subscription<px4_msgs::msg::VehicleStatus>::SharedPtr mode_sub;
    rclcpp::Subscription<utils::msg::UAVCommand>::SharedPtr command_sub;
    rclcpp::Subscription<utils::msg::UAVStateFeedback>::SharedPtr state_sub;
    rclcpp::Publisher<px4_msgs::msg::OffboardControlMode>::SharedPtr offboard_pub;
    rclcpp::Publisher<px4_msgs::msg::VehicleThrustSetpoint>::SharedPtr thrust_cmd_pub;
    rclcpp::Publisher<px4_msgs::msg::VehicleTorqueSetpoint>::SharedPtr torque_cmd_pub;

    void parameter_init();

    void parameter_update();

    void timer_callback();

    void publish_offboard_control_mode();

    void publish_controller_cmd();

    void state_sub_cb(const utils::msg::UAVStateFeedback::SharedPtr msg);

    void command_sub_cb(const utils::msg::UAVCommand::SharedPtr msg);
};

void Controller_node::parameter_init() {
    parameters_ = {{"J1",                0},
                   {"J2",                0},
                   {"J3",                0},
                   {"m",                 0},
                   {"g",                 9.81},
                   {"thrust_scale",      1},
                   {"torque_scale",      1},
                   {"use_decoupled_yaw", 0},
                   {"kX1",               0},
                   {"kX2",               0},
                   {"kX3",               0},
                   {"kV1",               0},
                   {"kV2",               0},
                   {"kV3",               0},
                   {"kR1",               0},
                   {"kR2",               0},
                   {"kR3",               0},
                   {"kW1",               0},
                   {"kW2",               0},
                   {"kW3",               0},
                   {"c_tf",              0},
                   {"l",                 0},
                   {"use_integral",      1},
                   {"kIX",               0},
                   {"ki ",               0},
                   {"kIR",               0},
                   {"kI ",               0},
                   {"kyI",               0},
                   {"c1 ",               0},
                   {"c2 ",               0},
                   {"c3 ",               0}

    };
    this->declare_parameters<double>("", parameters_);
}

void Controller_node::parameter_update() {
    if (this->get_parameters("", parameters_)) {
        geometric_param->J.diagonal() << parameters_["J1"], parameters_["J2"], parameters_["J3"];
        geometric_param->m = parameters_["m"];
        geometric_param->g = parameters_["g"];
        geometric_param->c_tf = parameters_["c_tf"];
        geometric_param->l = parameters_["l"];
        geometric_param->thrust_scale = parameters_["thrust_scale"];
        geometric_param->torque_scale = parameters_["torque_scale"];

        geometric_param->use_decoupled_yaw = int(parameters_["use_decoupled_yaw"]);
        geometric_param->kX.diagonal() << parameters_["kX1"], parameters_["kX2"], parameters_["kX3"];
        geometric_param->kV.diagonal() << parameters_["kV1"], parameters_["kV2"], parameters_["kV3"];
        geometric_param->kR.diagonal() << parameters_["kR1"], parameters_["kR2"], parameters_["kR3"];
        geometric_param->kW.diagonal() << parameters_["kW1"], parameters_["kW2"], parameters_["kW3"];

        geometric_param->use_integral = int(parameters_["use_integral"]);
        geometric_param->kIX = parameters_["kIX"];
        geometric_param->ki = parameters_["ki"];
        geometric_param->kIR = parameters_["kIR"];
        geometric_param->kI = parameters_["kI"];
        geometric_param->kyI = parameters_["kyI"];
        geometric_param->c1 = parameters_["c1"];
        geometric_param->c2 = parameters_["c2"];
        geometric_param->c3 = parameters_["c3"];
    }
}

void Controller_node::timer_callback() {
    timestamp.store(this->get_clock()->now().nanoseconds() / 1000);
    if (!is_posctl) {
        publish_controller_cmd();
        publish_offboard_control_mode();
    }
}

void Controller_node::publish_offboard_control_mode() {
    px4_msgs::msg::OffboardControlMode msg{};
    msg.position = false;
    msg.velocity = false;
    msg.acceleration = false;
    msg.attitude = false;
    msg.body_rate = false;
    msg.actuator = true;
    msg.timestamp = timestamp.load();
    offboard_pub->publish(msg);
}

void Controller_node::publish_controller_cmd() {
    Vector4d fM_cmd;
    controller->compute_control_output();
    controller->get_fM_cmd(fM_cmd, true);
    px4_msgs::msg::VehicleThrustSetpoint thrust_sp{};
    thrust_sp.timestamp = this->get_clock()->now().nanoseconds() / 1000;
    thrust_sp.timestamp_sample = this->get_clock()->now().nanoseconds() / 1000;//TODO:change to state feedback timestamp
    thrust_sp.xyz[0] = 0;
    thrust_sp.xyz[1] = 0;
    thrust_sp.xyz[2] = fM_cmd(0);
    thrust_cmd_pub->publish(thrust_sp);
    px4_msgs::msg::VehicleTorqueSetpoint torque_sp{};
    torque_sp.timestamp = this->get_clock()->now().nanoseconds() / 1000;
    torque_sp.timestamp_sample = this->get_clock()->now().nanoseconds() / 1000;
    torque_sp.xyz[0] = fM_cmd(1);
    torque_sp.xyz[1] = fM_cmd(2);
    torque_sp.xyz[2] = fM_cmd(3);
    torque_cmd_pub->publish(torque_sp);
}

void Controller_node::state_sub_cb(const utils::msg::UAVStateFeedback::SharedPtr msg) {
    state->timestamp.sec = msg->header.stamp.sec;
    state->timestamp.nanosec = msg->header.stamp.nanosec;
    state->world_frame = msg->world_frame;
    state->body_frame = msg->body_frame;
    state->x << msg->pos.x, msg->pos.y, msg->pos.z;
    state->x_dot << msg->vel.x, msg->vel.y, msg->vel.z;
    state->x_2dot << msg->acc.x, msg->acc.y, msg->acc.z;
    state->q = Quaterniond(msg->q.w, msg->q.x, msg->q.y, msg->q.z);
    state->w << msg->ang_vel.x, msg->ang_vel.y, msg->ang_vel.z;
}

void Controller_node::command_sub_cb(const utils::msg::UAVCommand::SharedPtr msg) {
    command->timestamp.sec = msg->header.stamp.sec;
    command->timestamp.nanosec = msg->header.stamp.nanosec;
    command->world_frame = msg->world_frame;
    command->body_frame = msg->body_frame;
    command->x << msg->pos.x, msg->pos.y, msg->pos.z;
    command->x_dot << msg->vel.x, msg->vel.y, msg->vel.z;
    command->x_2dot << msg->acc.x, msg->acc.y, msg->acc.z;
    command->x_3dot << msg->jerk.x, msg->jerk.y, msg->jerk.z;
    command->x_4dot << msg->snap.x, msg->snap.y, msg->snap.z;
    command->b1d << msg->heading.x, msg->heading.y, msg->heading.z;
    command->b1d_dot << msg->heading_dot.x, msg->heading_dot.y, msg->heading_dot.z;
    command->b1d_2dot << msg->heading_2dot.x, msg->heading_2dot.y, msg->heading_2dot.z;
}

int main(int argc, char *argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<Controller_node>());
    rclcpp::shutdown();
    return 0;
}