#ifndef __CONTROLLER_H
#define __CONTROLLER_H

#include "Eigen/Eigen"
#include "rclcpp/rclcpp.hpp"
#include "Structs.hpp"
#include "base_env/msg/uav_motion.hpp"

class Parameter_t
{
public:
	Eigen::Matrix3d Kp;
	Eigen::Matrix3d Kv;
	Eigen::Matrix3d Ki;

	double mass;
	double gra;
	double hov_percent;

	double i_limit_max;
	double i_c;

	double ctrl_rate;
};

struct attitude_sp_t
{
	float thrust;
	float q[4];
};

class Controller
{
private:
	Eigen::Vector3d int_e_v;

	Eigen::Quaterniond computeDesiredAttitude(const Eigen::Vector3d &desired_acceleration, const double reference_heading, const Eigen::Quaterniond &attitude_estimate) const;

	bool almostZero(const double value) const;
	bool almostZeroThrust(const double thrust_value) const;

	Eigen::Vector3d computeRobustBodyXAxis(const Eigen::Vector3d &x_B_prototype, const Eigen::Vector3d &x_C, const Eigen::Vector3d &y_C, const Eigen::Quaterniond &attitude_estimate) const;

	Eigen::Matrix3d rotz(double t);
	bool no_yaw_flag = 0;
	Eigen::Vector3d err_vel_i;
	double yaw_des = 0;
	double get_yaw_from_quaternion(const Eigen::Quaterniond &q);
	Eigen::Vector3d _thrust;
	Eigen::Vector3d _df;
	Eigen::Vector3d _last_integration;
public:
	Parameter_t param;
	void run(UAV_motion_t &motion_input, UAV_motion_t &motion_fb, attitude_sp_t &u);
	Eigen::Vector3d getDesiredThrust()
	{
		return _thrust;
	}
	void setDisturbance(Eigen::Vector3d &disturbance_F)
	{
		_df = disturbance_F;
	}
};

#endif
