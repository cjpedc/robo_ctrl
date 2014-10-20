#include "ros/ros.h"
#include "std_msgs/Float64.h"
#include "std_msgs/Int32.h"
#include <ras_arduino_msgs/PWM.h>
#include <geometry_msgs/Twist.h>
#include <sstream>

int main(int argc, char **argv)
{
 
  ros::init(argc, argv, "linear_cartesian_controller");
  ros::NodeHandle n;

  ros::Publisher twist = n.advertise<geometry_msgs::Twist>("/motor_controller/twist", 1000);

  ros::Rate loop_rate(10);

  geometry_msgs::Twist msg;

  msg.linear.x = .5; // in [m/s]
  msg.angular.z = 0; //


  while (ros::ok())
  {

    twist.publish(msg);

    ros::spinOnce();

    loop_rate.sleep();

  }


  return 0;
}
