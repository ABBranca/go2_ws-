// map_odom_broadcaster — publishes a dynamic map→odom transform.
//
// Initial behavior: identity. Subscribes to /initialpose as a forward-
// compatibility hook so a future loop-closure / relocalization layer can
// update the published transform without touching the launch graph.
//
// REP-105: map and odom are world-fixed frames; map can jump on relocalization,
// odom is continuous. With FAST-LIO2 (no loop closure), map ≡ odom is the
// physically correct initial state.

#include <chrono>
#include <memory>

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>
#include <tf2_ros/transform_broadcaster.h>

using namespace std::chrono_literals;

class MapOdomBroadcaster : public rclcpp::Node {
public:
  MapOdomBroadcaster() : Node("map_odom_broadcaster") {
    publish_rate_hz_ = declare_parameter<double>("publish_rate_hz", 50.0);
    map_frame_ = declare_parameter<std::string>("map_frame", "map");
    odom_frame_ = declare_parameter<std::string>("odom_frame", "odom");

    transform_.header.frame_id = map_frame_;
    transform_.child_frame_id = odom_frame_;
    transform_.transform.rotation.w = 1.0;  // identity

    broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);

    const auto period = std::chrono::duration<double>(1.0 / publish_rate_hz_);
    timer_ = create_wall_timer(
      std::chrono::duration_cast<std::chrono::nanoseconds>(period),
      [this]() { publish(); });

    initialpose_sub_ = create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
      "/initialpose", 10,
      [this](const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg) {
        on_initialpose(*msg);
      });

    RCLCPP_INFO(get_logger(),
      "map_odom_broadcaster active: %s -> %s @ %.1f Hz",
      map_frame_.c_str(), odom_frame_.c_str(), publish_rate_hz_);
  }

private:
  void publish() {
    transform_.header.stamp = now();
    broadcaster_->sendTransform(transform_);
  }

  void on_initialpose(const geometry_msgs::msg::PoseWithCovarianceStamped & msg) {
    // Forward-compatibility stub: future loop closure / relocalization
    // layer publishes here to nudge map->odom. Currently a passive update.
    const auto & p = msg.pose.pose;
    transform_.transform.translation.x = p.position.x;
    transform_.transform.translation.y = p.position.y;
    transform_.transform.translation.z = p.position.z;
    transform_.transform.rotation = p.orientation;
    RCLCPP_INFO(get_logger(),
      "map->odom updated from /initialpose: t=(%.3f, %.3f, %.3f)",
      p.position.x, p.position.y, p.position.z);
  }

  double publish_rate_hz_;
  std::string map_frame_;
  std::string odom_frame_;
  geometry_msgs::msg::TransformStamped transform_;
  std::unique_ptr<tf2_ros::TransformBroadcaster> broadcaster_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr initialpose_sub_;
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MapOdomBroadcaster>());
  rclcpp::shutdown();
  return 0;
}
