#include "ros/ros.h"
#include "std_msgs/Float64.h"
#include "std_msgs/Int32.h"
#include <ras_arduino_msgs/PWM.h>
#include <ras_arduino_msgs/Encoders.h>
#include <geometry_msgs/Twist.h>
#include <sstream>
#include <iostream>
#include <cmath>

static const double pi = acos(-1.0);

class motor_controller
{
private:

	const int ticks; //ticks_per_rev
	double Kp[2]; // P gain {left, right}	//DON'T CHANGE THIS DURING RUNTIME
	double Ti[2];	//DON'T CHANGE THIS DURING RUNTIME
	double e[2]; // error
	double e_prev[2]; //previous error
	double du[2]; // delta control signal 'delta_u'
	double wlr_[2];
	double pwm_[2];
	double actual_pwm[2];

public:

	ros::NodeHandle n_;
	ros::Subscriber twist_subscriber_;
	ros::Subscriber encoders_subscriber_;
	ros::Publisher pwm_publisher_;
	double twist_[2];
	double encoder_[5];
	const double control_frequency; // Control @ 10 Hz
	const double b; // separation of the two central wheels in [m]
	const double r; //  wheel radius in [m]

	motor_controller() : ticks(360), b(0.206), r(0.0485), control_frequency(10.0)
	{
		Kp[0] = 1.085; //0.09;
		Kp[1] = 1.14;//0.103;
		Ti[0] = 0.103;//0.09;
		Ti[1] = 0.106;//0.0855;
		wlr_[0] = 0;
		wlr_[1] = 0;
		pwm_[0] = 0;
		pwm_[1] = 0;
		actual_pwm[0] = 65;
		actual_pwm[1] = 70;
		e_prev[0] = 0;
		e_prev[1] = 0;
		n_ = ros::NodeHandle("~");
		twist_subscriber_ = n_.subscribe("/motor_controller/twist", 1, &motor_controller::twistCallback, this);
		encoders_subscriber_ = n_.subscribe("/arduino/encoders", 1, &motor_controller::encodersCallback, this);
		pwm_publisher_ = n_.advertise<ras_arduino_msgs::PWM>("/arduino/pwm", 1000);
	}

	void twistCallback(const geometry_msgs::Twist::ConstPtr &msg)
	{
		twist_[0] = msg->linear.x;
		twist_[1] = msg->angular.z;
	}

	void encodersCallback(const ras_arduino_msgs::Encoders::ConstPtr &msg)
	{
		encoder_[0] = msg->encoder1;
		encoder_[1] = msg->encoder2;
		encoder_[2] = msg->delta_encoder1;
		encoder_[3] = msg->delta_encoder2;
		encoder_[4] = msg->timestamp;
	}

	// wheel angular velocity
	void wheels_ang_vel ()
	{
		double v = twist_[0]; // robot linear velocity
		double w = twist_[1]; // robot angular velocity
		wlr_[0] = (v - (b/2) * w)/r; //left wheel angular velocity
		wlr_[1] = (v + (b/2) * w)/r; //right wheel angular velocity
	}

	// Update pwm signals
	void computePwm ()
	{
		std::vector<double> estimated_w_;
		estimated_w_ = std::vector<double>(2,0);

		double delta_encoder[2];
		delta_encoder[0] = (double) motor_controller::encoder_[3];
		delta_encoder[1] = (double) motor_controller::encoder_[2];

		std::cerr << "delta_encoder[0] = " << delta_encoder[0] << std::endl;
		std::cerr << "delta_encoder[1] = " << delta_encoder[1] << std::endl;

		/*
		estimated_w = (delta_encoder*2*pi*control_frequency)/(ticks_per_rev)
		pwm = pwm + alpha*(desired_w - estimated_w)
		*/

		for (int i = 0;i<2;++i) {
			// Estimation
			estimated_w_[i] = (delta_encoder[i]*2*pi*control_frequency)/ticks;

			// Error calculation
			e[i] = wlr_[i] - estimated_w_[i];

			// control signal change
			du[i] = Kp[i] * ((e[i] - e_prev[i]) + (1.0/control_frequency) * e[i] / Ti[i]);

			e_prev[i] = e[i];

			// control signal update
			pwm_[i] = actual_pwm[i] + du[i];
			if (pwm_[i] > 255.0) {
				pwm_[i] -= du[i];
			}
			if (pwm_[i] < -255.0) {
				pwm_[i] -= du[i];
			}
			
			actual_pwm[i] = pwm_[i];
		}
		
		std::cerr << "Ti[1] is " << Ti[1] << std::endl;
		std::cerr << "du[0] is " << du[0] << std::endl;
		std::cerr << "du[1] is " << du[1] << std::endl;
		std::cerr << "difference for left wheel is " << wlr_[0] << "-" << estimated_w_[0] << std::endl;
		std::cerr << "difference for right wheel is " << wlr_[1] << "-" << estimated_w_[1] << std::endl;
		std::cerr << "Left W: " << estimated_w_[0] << std::endl;
		std::cerr << "Right W: " << estimated_w_[1] << std::endl;
		std::cerr << "wl: " << wlr_[0] << std::endl;
		std::cerr << "wr: " << wlr_[1] << std::endl;
	}

	void publish()
	{
		wheels_ang_vel();

		std::cerr << "Left Wheel: " << wlr_[0] << std::endl;
		std::cerr << "Right Wheel: " << wlr_[1] << std::endl;

		computePwm();

		std::cerr << "Left PWM: " << pwm_[0] << std::endl;
		std::cerr << "Right PWM: " << pwm_[1] << std::endl;

		ras_arduino_msgs::PWM msg;
		msg.PWM1 = (int)pwm_[0];
		msg.PWM2 = (int)pwm_[1];
		pwm_publisher_.publish(msg);
	}
};



int main(int argc, char **argv)
{

	ros::init(argc, argv, "motor_controller");
	motor_controller motor_controller_node;
	ros::Rate loop_rate(motor_controller_node.control_frequency);

  while (ros::ok())
	{
		motor_controller_node.publish();
		ros::spinOnce();
		loop_rate.sleep();
  }

  return 0;
}
