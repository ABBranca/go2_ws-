#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <unitree_go/msg/low_cmd.hpp> // Assuming Unitree Go2 SDK message type

class Go2NavBridge : public rclcpp::Node {
public:
  Go2NavBridge() : Node("go2_nav_bridge") {
    // Subscribe to Nav2 cmd_vel
    cmd_vel_sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
      "cmd_vel", 10, std::bind(&Go2NavBridge::cmd_vel_callback, this, std::placeholders::_1));

    // Publisher for Unitree SDK (High-level or Low-level control)
    // NOTE: This should match the specific Unitree SDK message for Go2
    sdk_pub_ = this->create_publisher<unitree_go::msg::LowCmd>("low_cmd", 10);

    RCLCPP_INFO(this->get_logger(), "Unitree Go2 Navigation Bridge Started.");
  }

private:
  void cmd_vel_callback(const geometry_msgs::msg::Twist::SharedPtr msg) {
    // Translation logic goes here
    // Example: Map linear.x and angular.z to robot movement
    auto sdk_msg = unitree_go::msg::LowCmd();
    
    // TODO: Map Twist values to unitree_go message fields
    // This depends on the exact version of the Unitree SDK used
    
    sdk_pub_->publish(sdk_msg);
  }

  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_;
  rclcpp::Publisher<unitree_go::msg::LowCmd>::SharedPtr sdk_pub_;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<Go2NavBridge>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
