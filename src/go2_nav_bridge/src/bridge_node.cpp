// Copyright 2026 Alessandro Biagio Branca
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <mutex>
#include <thread>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp/timer.hpp>

#include <geometry_msgs/msg/twist.hpp>
#include <rcl_interfaces/msg/parameter_descriptor.hpp>
#include <unitree_go/msg/sport_mode_cmd.hpp>

#include <go2_nav_bridge/bridge_utils.hpp>

// Workaround: clangd 14 segfaults on RCLCPP_*_THROTTLE macros
#ifdef __clang__
#define SAFE_WARN_THROTTLE(logger, clock, period, ...)                         \
  RCLCPP_WARN(logger, __VA_ARGS__)
#define SAFE_ERROR_THROTTLE(logger, clock, period, ...)                        \
  RCLCPP_ERROR(logger, __VA_ARGS__)
#else
#define SAFE_WARN_THROTTLE(logger, clock, period, ...)                         \
  RCLCPP_WARN_THROTTLE(logger, clock, period, __VA_ARGS__)
#define SAFE_ERROR_THROTTLE(logger, clock, period, ...)                        \
  RCLCPP_ERROR_THROTTLE(logger, clock, period, __VA_ARGS__)
#endif

class Go2NavBridge : public rclcpp::Node {
public:
  Go2NavBridge()
      : Node("go2_nav_bridge"), cmd_vel_received_(false),
        timeout_stop_sent_(false) {

    auto cmd_vel_qos = rclcpp::QoS(10).reliable();
    // Freshness matters more than historical reliability for robot commands.
    auto sdk_qos = rclcpp::QoS(1).best_effort();

    cmd_vel_sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
        "cmd_vel", cmd_vel_qos,
        std::bind(&Go2NavBridge::cmd_vel_callback, this,
                  std::placeholders::_1));

    sdk_pub_ = this->create_publisher<unitree_go::msg::SportModeCmd>(
        "sport_mode_cmd", sdk_qos);

    rcl_interfaces::msg::ParameterDescriptor ro_desc;
    ro_desc.read_only = true;

    ro_desc.description = "Maximum linear velocity (m/s)";
    linear_max_ = validate_positive_parameter(
        "linear_max",
        this->declare_parameter("linear_max", go2_nav_bridge::kLinearMax,
                                ro_desc),
        go2_nav_bridge::kLinearMax);

    ro_desc.description = "Maximum angular velocity (rad/s)";
    angular_max_ = validate_positive_parameter(
        "angular_max",
        this->declare_parameter("angular_max", go2_nav_bridge::kAngularMax,
                                ro_desc),
        go2_nav_bridge::kAngularMax);

    ro_desc.description = "Watchdog timeout before stopping the robot (ms)";
    watchdog_timeout_ms_ = validate_positive_parameter(
        "watchdog_timeout_ms",
        this->declare_parameter("watchdog_timeout_ms",
                                go2_nav_bridge::kDefaultWatchdogMs, ro_desc),
        go2_nav_bridge::kDefaultWatchdogMs);

    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      last_cmd_vel_time_ = this->now();
      last_stop_publish_time_ =
          rclcpp::Time(0, 0, this->get_clock()->get_clock_type());
    }

    auto watchdog_period =
        go2_nav_bridge::watchdog_period_from_timeout(watchdog_timeout_ms_);
    if (watchdog_period.count() == 1 && watchdog_timeout_ms_ / 2.0 < 1.0) {
      RCLCPP_WARN(this->get_logger(),
                  "Parameter 'watchdog_timeout_ms' too small; "
                  "clamped timer period to 1 ms.");
    }

    watchdog_timer_ = this->create_wall_timer(
        watchdog_period, std::bind(&Go2NavBridge::watchdog_callback, this));

    publish_stop_command();

    RCLCPP_INFO(this->get_logger(),
                "Go2 Nav Bridge started — linear_max=%.2f m/s, "
                "angular_max=%.2f rad/s, watchdog=%.0f ms",
                linear_max_, angular_max_, watchdog_timeout_ms_);
  }

  ~Go2NavBridge() override {
    watchdog_timer_->cancel();
    if (sdk_pub_) {
      std::lock_guard<std::mutex> lock(state_mutex_);
      publish_stop_unlocked();
    }
    RCLCPP_INFO(this->get_logger(), "Go2 Nav Bridge stopped.");
  }

  void send_stop() {
    if (sdk_pub_) {
      std::lock_guard<std::mutex> lock(state_mutex_);
      publish_stop_unlocked();
    }
  }

private:
  void cmd_vel_callback(const geometry_msgs::msg::Twist::ConstSharedPtr &msg) {
    try {
      cmd_vel_received_ = true;
      timeout_stop_sent_ = false;
      {
        std::lock_guard<std::mutex> lock(state_mutex_);
        last_cmd_vel_time_ = this->now();
      }

      const double linear_x = sanitize_finite_input(msg->linear.x, "linear.x");
      const double linear_y = sanitize_finite_input(msg->linear.y, "linear.y");
      const double angular_z =
          sanitize_finite_input(msg->angular.z, "angular.z");

      const auto sdk_msg = make_velocity_command(
          go2_nav_bridge::clamp_symmetric(linear_x, linear_max_),
          go2_nav_bridge::clamp_symmetric(linear_y, linear_max_),
          go2_nav_bridge::clamp_symmetric(angular_z, angular_max_));

      sdk_pub_->publish(sdk_msg);
    } catch (const std::exception &e) {
      SAFE_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                          "Exception in cmd_vel_callback: %s", e.what());
    }
  }

  double validate_positive_parameter(const char *name, double value,
                                     double fallback) {
    if (!std::isfinite(value) || value <= 0.0) {
      RCLCPP_WARN(this->get_logger(),
                  "Invalid parameter '%s'=%.3f. Falling back to %.3f.", name,
                  value, fallback);
      return fallback;
    }

    return value;
  }

  double sanitize_finite_input(double value, const char *field_name) {
    if (!std::isfinite(value)) {
      SAFE_WARN_THROTTLE(
          this->get_logger(), *this->get_clock(), 1000,
          "Received non-finite cmd_vel '%s'; replacing with 0.0.", field_name);
      return 0.0;
    }

    return value;
  }

  unitree_go::msg::SportModeCmd
  make_velocity_command(float linear_x, float linear_y, float yaw_speed) {
    auto sdk_msg = unitree_go::msg::SportModeCmd();
    sdk_msg.mode = go2_nav_bridge::kVelocityMoveMode;
    sdk_msg.gait_type = 0U;
    sdk_msg.speed_level = 0U;
    sdk_msg.foot_raise_height = 0.0F;
    sdk_msg.body_height = 0.0F;
    sdk_msg.position[0] = 0.0F;
    sdk_msg.position[1] = 0.0F;
    sdk_msg.position[2] = 0.0F;
    sdk_msg.euler[0] = 0.0F;
    sdk_msg.euler[1] = 0.0F;
    sdk_msg.euler[2] = 0.0F;
    sdk_msg.velocity[0] = linear_x;
    sdk_msg.velocity[1] = linear_y;
    sdk_msg.yaw_speed = yaw_speed;
    return sdk_msg;
  }

  /// Publish zero-velocity command. Caller MUST hold state_mutex_.
  void publish_stop_unlocked() {
    sdk_pub_->publish(make_velocity_command(0.0F, 0.0F, 0.0F));
    last_stop_publish_time_ = this->now();
  }

  void publish_stop_command() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    publish_stop_unlocked();
  }

  void watchdog_callback() {
    try {
      const auto now = this->now();

      rclcpp::Time last_cmd_vel_time;
      rclcpp::Time last_stop_publish_time;
      {
        std::lock_guard<std::mutex> lock(state_mutex_);
        last_cmd_vel_time = last_cmd_vel_time_;
        last_stop_publish_time = last_stop_publish_time_;
      }

      const bool has_cmd_vel = cmd_vel_received_.load();
      const double elapsed_ms = (now - last_cmd_vel_time).seconds() * 1000.0;
      const bool timed_out =
          !has_cmd_vel || go2_nav_bridge::watchdog_timeout_elapsed(
                              elapsed_ms, watchdog_timeout_ms_);

      if (!timed_out) {
        timeout_stop_sent_ = false;
        return;
      }

      const bool first_timeout = !timeout_stop_sent_.exchange(true);
      const double since_last_stop_ms =
          (now - last_stop_publish_time).seconds() * 1000.0;
      const bool periodic_republish =
          since_last_stop_ms >= go2_nav_bridge::kStopRepublishMs;

      if (first_timeout || periodic_republish) {
        publish_stop_command();
        SAFE_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                           "cmd_vel timeout — publishing zero velocity.");
      }
    } catch (const std::exception &e) {
      SAFE_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                          "Exception in watchdog_callback: %s", e.what());
    }
  }

  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_;
  rclcpp::Publisher<unitree_go::msg::SportModeCmd>::SharedPtr sdk_pub_;
  rclcpp::TimerBase::SharedPtr watchdog_timer_;

  std::mutex state_mutex_;
  std::atomic<bool> cmd_vel_received_;
  std::atomic<bool> timeout_stop_sent_;
  rclcpp::Time last_cmd_vel_time_;
  rclcpp::Time last_stop_publish_time_;

  double linear_max_;
  double angular_max_;
  double watchdog_timeout_ms_;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<Go2NavBridge>();

  rclcpp::on_shutdown([weak = std::weak_ptr<Go2NavBridge>(node)]() {
    if (auto n = weak.lock()) {
      n->send_stop();
      // Allow time for DDS to flush the stop command before transport teardown
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  });

  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
