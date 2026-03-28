#include <algorithm>
#include <cstdint>
#include <functional>
#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <rclcpp/timer.hpp>
#include <unitree_go/msg/sport_mode_cmd.hpp>

static constexpr double kLinearMax  = 1.0;   // m/s
static constexpr double kAngularMax = 1.0;   // rad/s
static constexpr uint8_t kVelocityMoveMode = 2;
static constexpr double kWatchdogMs = 200.0; // ms

class Go2NavBridge : public rclcpp::Node {
public:
  Go2NavBridge() : Node("go2_nav_bridge"), cmd_vel_received_(false) {
    // Nav2 publishes cmd_vel with RELIABLE QoS
    auto qos = rclcpp::QoS(10).reliable();

    cmd_vel_sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
      "cmd_vel", qos,
      std::bind(&Go2NavBridge::cmd_vel_callback, this, std::placeholders::_1));

    sdk_pub_ = this->create_publisher<unitree_go::msg::SportModeCmd>("sport_mode_cmd", qos);

    // Safety watchdog: publish zero velocity if no cmd_vel received within timeout
    watchdog_timer_ = this->create_wall_timer(
      std::chrono::milliseconds(static_cast<int>(kWatchdogMs)),
      std::bind(&Go2NavBridge::watchdog_callback, this));

    RCLCPP_INFO(this->get_logger(), "Unitree Go2 Navigation Bridge started.");
  }

private:
  void cmd_vel_callback(const geometry_msgs::msg::Twist::ConstSharedPtr & msg) {
    cmd_vel_received_ = true;

    auto sdk_msg = unitree_go::msg::SportModeCmd();
    sdk_msg.mode       = kVelocityMoveMode;
    sdk_msg.velocity[0] = static_cast<float>(
      std::clamp(msg->linear.x,  -kLinearMax,  kLinearMax));
    sdk_msg.velocity[1] = static_cast<float>(
      std::clamp(msg->linear.y,  -kLinearMax,  kLinearMax));
    sdk_msg.yaw_speed   = static_cast<float>(
      std::clamp(msg->angular.z, -kAngularMax, kAngularMax));

    sdk_pub_->publish(sdk_msg);
  }

  void watchdog_callback() {
    if (!cmd_vel_received_) {
      // No cmd_vel received since last watchdog tick — publish zero velocity
      auto stop_msg = unitree_go::msg::SportModeCmd();
      stop_msg.mode = kVelocityMoveMode;
      stop_msg.velocity[0] = 0.0f;
      stop_msg.velocity[1] = 0.0f;
      stop_msg.yaw_speed   = 0.0f;
      sdk_pub_->publish(stop_msg);
      RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
        "cmd_vel timeout — publishing zero velocity.");
    }
    cmd_vel_received_ = false;
  }

  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_;
  rclcpp::Publisher<unitree_go::msg::SportModeCmd>::SharedPtr sdk_pub_;
  rclcpp::TimerBase::SharedPtr watchdog_timer_;
  
  bool cmd_vel_received_;
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<Go2NavBridge>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
