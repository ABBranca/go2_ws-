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
static constexpr double kDefaultWatchdogMs = 200.0; // ms

class Go2NavBridge : public rclcpp::Node {
public:
  Go2NavBridge() : Node("go2_nav_bridge"), cmd_vel_received_(false) {
    // Nav2 publishes cmd_vel with RELIABLE QoS
    auto qos = rclcpp::QoS(10).reliable();

    cmd_vel_sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
      "cmd_vel", qos,
      std::bind(&Go2NavBridge::cmd_vel_callback, this, std::placeholders::_1));

    sdk_pub_ = this->create_publisher<unitree_go::msg::SportModeCmd>("sport_mode_cmd", qos);

    // Read parameters (flexible tuning from launch files)
    linear_max_ = this->declare_parameter("linear_max", kLinearMax);
    angular_max_ = this->declare_parameter("angular_max", kAngularMax);
    watchdog_timeout_ms_ = this->declare_parameter("watchdog_timeout_ms", kDefaultWatchdogMs);
    last_cmd_vel_time_ = this->now();

    // Safety watchdog: publish zero velocity if no cmd_vel received within timeout
    watchdog_timer_ = this->create_wall_timer(
      std::chrono::milliseconds(static_cast<int>(watchdog_timeout_ms_)),
      std::bind(&Go2NavBridge::watchdog_callback, this));

    RCLCPP_INFO(this->get_logger(), "Unitree Go2 Navigation Bridge started.");
  }

private:
  void cmd_vel_callback(const geometry_msgs::msg::Twist::ConstSharedPtr & msg) {
    cmd_vel_received_ = true;
    last_cmd_vel_time_ = this->now();

    auto sdk_msg = unitree_go::msg::SportModeCmd();
    sdk_msg.mode       = kVelocityMoveMode;
    sdk_msg.velocity[0] = static_cast<float>(
      std::clamp(msg->linear.x,  -linear_max_,  linear_max_));
    sdk_msg.velocity[1] = static_cast<float>(
      std::clamp(msg->linear.y,  -linear_max_,  linear_max_));
    sdk_msg.yaw_speed   = static_cast<float>(
      std::clamp(msg->angular.z, -angular_max_, angular_max_));

    sdk_pub_->publish(sdk_msg);
  }

  void watchdog_callback() {
    if (!cmd_vel_received_ || (this->now() - last_cmd_vel_time_).nanoseconds() / 1e6 > watchdog_timeout_ms_) {
      auto stop_msg = unitree_go::msg::SportModeCmd();
      stop_msg.mode = kVelocityMoveMode;
      stop_msg.velocity[0] = 0.0f;
      stop_msg.velocity[1] = 0.0f;
      stop_msg.yaw_speed   = 0.0f;
      sdk_pub_->publish(stop_msg);
      RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
        "cmd_vel timeout — publishing zero velocity.");
      cmd_vel_received_ = false;
      return;
    }

    // cmd_vel is fresh
  }

  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_;
  rclcpp::Publisher<unitree_go::msg::SportModeCmd>::SharedPtr sdk_pub_;
  rclcpp::TimerBase::SharedPtr watchdog_timer_;
  
  bool cmd_vel_received_;
  rclcpp::Time last_cmd_vel_time_;
  double linear_max_;
  double angular_max_;
  double watchdog_timeout_ms_;
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<Go2NavBridge>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
