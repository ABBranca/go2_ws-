// odom_tf_broadcaster — republishes the Go2 onboard odometry as an odom→base_link TF,
// and (optionally) a gravity-levelled base_link→base_stabilized frame.
//
// The Unitree Go2 Expansion Dock publishes a fused leg-kinematic + IMU odometry
// on /utlidar/robot_odom (nav_msgs/Odometry). Its key virtue here: leg/contact
// kinematics anchor the body height to the floor, so Z stays bounded — unlike a
// 16-beam horizontal LiDAR-inertial estimator (FAST-LIO2), whose Z is
// unobservable and runs away. We use this odometry as the continuous odom→base_link
// transform; slam_toolbox then supplies the map→odom correction from 2D scan matching.
//
// REP-105: odom→base_link must be smooth/continuous (this node), map→odom may jump
// on loop closure (slam_toolbox). We deliberately do NOT broadcast a TF directly
// from the firmware to keep frame names under our control.
//
// base_stabilized (gait-tilt compensation):
//   pointcloud_to_laserscan flattens the horizontal XT16 into 2D. Doing so in
//   base_link — which pitches/rolls with the gait — bleeds floor/ceiling returns
//   into /scan. We publish base_link→base_stabilized (roll/pitch removed, yaw kept)
//   so the flattener can project in a level slab. Two attitude sources:
//     tilt_source=odom : split roll/pitch from the LEG-ODOM orientation. Simple, but
//                        the Go2 leg-odom attitude carries ~−2.6° static roll bias +
//                        noise → REGRESSED the map in testing (2026-06-03).
//     tilt_source=imu  : derive roll/pitch from /utlidar/imu (gravity-referenced,
//                        drift-bounded). Either consume a firmware-provided orientation
//                        quaternion, or fuse raw accel+gyro with a complementary filter.
//   Z stays unobservable by the LiDAR (see z-observability); this is a *projection*
//   fix, not a vertical state estimate.

#include <cmath>
#include <memory>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>
#include <tf2_ros/transform_broadcaster.h>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Matrix3x3.h>

class OdomTfBroadcaster : public rclcpp::Node {
public:
  OdomTfBroadcaster() : Node("odom_tf_broadcaster") {
    input_topic_ = declare_parameter<std::string>("input_topic", "/utlidar/robot_odom");
    odom_frame_  = declare_parameter<std::string>("odom_frame", "odom");
    base_frame_  = declare_parameter<std::string>("base_frame", "base_link");
    // Some firmware leaves the odometry Z noisy; optionally flatten to a fixed height.
    lock_z_      = declare_parameter<bool>("lock_z", false);
    fixed_z_     = declare_parameter<double>("fixed_z", 0.0);
    // Gravity-stabilised frame: base_link with roll/pitch removed (yaw kept).
    publish_stabilized_ = declare_parameter<bool>("publish_stabilized", true);
    stabilized_frame_   = declare_parameter<std::string>("stabilized_frame", "base_stabilized");
    // Attitude source for the de-tilt: "odom" (leg-odom orientation) or "imu".
    tilt_source_ = declare_parameter<std::string>("tilt_source", "odom");
    // IMU options (used only when tilt_source == "imu").
    imu_topic_   = declare_parameter<std::string>("imu_topic", "/utlidar/imu");
    // "auto"          : pick by orientation_covariance[0] on the first message
    // "quaternion"    : trust the firmware orientation quaternion
    // "complementary" : fuse raw accel (gravity) + gyro (rate) for roll/pitch
    imu_mode_    = declare_parameter<std::string>("imu_mode", "auto");
    cf_tau_      = declare_parameter<double>("cf_tau", 0.25);   // CF time constant [s]
    roll_offset_  = declare_parameter<double>("roll_offset", 0.0);   // static bias [rad]
    pitch_offset_ = declare_parameter<double>("pitch_offset", 0.0);  // static bias [rad]
    imu_qos_reliable_ = declare_parameter<bool>("imu_qos_reliable", false);

    broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);

    sub_ = create_subscription<nav_msgs::msg::Odometry>(
      input_topic_, rclcpp::SensorDataQoS(),
      [this](const nav_msgs::msg::Odometry::SharedPtr msg) { on_odom(*msg); });

    use_imu_tilt_ = publish_stabilized_ && (tilt_source_ == "imu");
    if (use_imu_tilt_) {
      auto qos = imu_qos_reliable_
        ? rclcpp::QoS(rclcpp::KeepLast(50)).reliable()
        : rclcpp::QoS(rclcpp::SensorDataQoS());
      imu_sub_ = create_subscription<sensor_msgs::msg::Imu>(
        imu_topic_, qos,
        [this](const sensor_msgs::msg::Imu::SharedPtr msg) { on_imu(*msg); });
      resolved_mode_ = imu_mode_;   // may be refined from "auto" on the first message
    }

    RCLCPP_INFO(get_logger(),
      "odom_tf_broadcaster: %s -> %s from '%s' (lock_z=%s, tilt_source=%s%s)",
      odom_frame_.c_str(), base_frame_.c_str(), input_topic_.c_str(),
      lock_z_ ? "true" : "false", tilt_source_.c_str(),
      use_imu_tilt_ ? (", imu_topic=" + imu_topic_).c_str() : "");
  }

private:
  void on_odom(const nav_msgs::msg::Odometry & msg) {
    geometry_msgs::msg::TransformStamped t;
    t.header.stamp = msg.header.stamp;
    t.header.frame_id = odom_frame_;
    t.child_frame_id = base_frame_;
    t.transform.translation.x = msg.pose.pose.position.x;
    t.transform.translation.y = msg.pose.pose.position.y;
    t.transform.translation.z = lock_z_ ? fixed_z_ : msg.pose.pose.position.z;
    t.transform.rotation = msg.pose.pose.orientation;
    broadcaster_->sendTransform(t);

    // base_stabilized from the leg-odom attitude. Skipped when the IMU owns the
    // de-tilt (tilt_source == "imu"); see on_imu().
    if (publish_stabilized_ && !use_imu_tilt_) {
      publish_stabilized_from_odom(msg);
    }
  }

  // Broadcast base_link -> base_stabilized = the rotation that removes the body's
  // roll/pitch while preserving yaw, using the LEG-ODOM orientation. Derivation: with
  // q = odom->base_link, split off yaw as q_yaw; the residual tilt is
  // q_tilt = q_yaw^-1 * q. The level frame shares q_yaw with odom, so
  // base_link->base_stabilized = q^-1 * q_yaw = q_tilt^-1.
  void publish_stabilized_from_odom(const nav_msgs::msg::Odometry & msg) {
    tf2::Quaternion q(
      msg.pose.pose.orientation.x, msg.pose.pose.orientation.y,
      msg.pose.pose.orientation.z, msg.pose.pose.orientation.w);
    if (q.length2() < 1e-9) { return; }              // guard against an all-zero quat
    q.normalize();

    double roll, pitch, yaw;
    tf2::Matrix3x3(q).getRPY(roll, pitch, yaw);

    tf2::Quaternion q_yaw;  q_yaw.setRPY(0.0, 0.0, yaw);
    tf2::Quaternion q_bl_to_stab = (q_yaw.inverse() * q).inverse();   // = q_tilt^-1
    send_stabilized(msg.header.stamp, q_bl_to_stab);
  }

  // Broadcast base_link -> base_stabilized from the IMU. Roll/pitch are gravity-
  // referenced (drift-bounded); yaw is deliberately discarded (no magnetometer → IMU
  // yaw drifts; heading stays owned by odom + slam_toolbox). The level frame is the
  // body with the corrected tilt undone: base_link->base_stabilized = q_tilt^-1.
  void on_imu(const sensor_msgs::msg::Imu & msg) {
    // Resolve "auto" once, on the first message, from the orientation covariance flag.
    if (resolved_mode_ == "auto") {
      const bool have_orientation = (msg.orientation_covariance[0] >= 0.0);
      resolved_mode_ = have_orientation ? "quaternion" : "complementary";
      RCLCPP_INFO(get_logger(),
        "tilt_source=imu: resolved imu_mode='%s' (orientation_covariance[0]=%.3g)",
        resolved_mode_.c_str(), msg.orientation_covariance[0]);
    }

    double roll = 0.0, pitch = 0.0;
    if (resolved_mode_ == "quaternion") {
      tf2::Quaternion q(msg.orientation.x, msg.orientation.y,
                        msg.orientation.z, msg.orientation.w);
      if (q.length2() < 1e-9) { return; }            // firmware did not fill orientation
      q.normalize();
      double yaw_unused;
      tf2::Matrix3x3(q).getRPY(roll, pitch, yaw_unused);
    } else {
      // Complementary filter: gyro (high-frequency) + accel-gravity (low-frequency).
      const double ax = msg.linear_acceleration.x;
      const double ay = msg.linear_acceleration.y;
      const double az = msg.linear_acceleration.z;
      // Gravity-vector tilt (drift-free, but corrupted by gait linear acceleration).
      const double roll_acc  = std::atan2(ay, az);
      const double pitch_acc = std::atan2(-ax, std::sqrt(ay * ay + az * az));

      const rclcpp::Time stamp(msg.header.stamp);
      if (!have_cf_state_) {
        roll_ = roll_acc;                            // seed from gravity on first sample
        pitch_ = pitch_acc;
        last_imu_time_ = stamp;
        have_cf_state_ = true;
        return;                                      // no dt yet — wait for the next msg
      }
      const double dt = (stamp - last_imu_time_).seconds();
      last_imu_time_ = stamp;
      if (dt <= 0.0 || dt > 0.1) {                   // reordered / gapped stamps: resync
        roll_ = roll_acc;
        pitch_ = pitch_acc;
        roll = roll_; pitch = pitch_;
      } else {
        // gyro integration (high-pass) blended with accel gravity (low-pass)
        const double alpha = cf_tau_ / (cf_tau_ + dt);   // = τ/(τ+dt) ∈ (0,1)
        roll_  = alpha * (roll_  + msg.angular_velocity.x * dt) + (1.0 - alpha) * roll_acc;
        pitch_ = alpha * (pitch_ + msg.angular_velocity.y * dt) + (1.0 - alpha) * pitch_acc;
        roll = roll_; pitch = pitch_;
      }
    }

    // Remove the static mounting/bias offset (captured while standing level).
    const double roll_corr  = roll  - roll_offset_;
    const double pitch_corr = pitch - pitch_offset_;

    tf2::Quaternion q_tilt;  q_tilt.setRPY(roll_corr, pitch_corr, 0.0);
    tf2::Quaternion q_bl_to_stab = q_tilt.inverse();
    send_stabilized(msg.header.stamp, q_bl_to_stab);
  }

  // Shared tail: emit base_link -> stabilized_frame_ (same origin, only de-tilted).
  void send_stabilized(const builtin_interfaces::msg::Time & stamp,
                       tf2::Quaternion q_bl_to_stab) {
    q_bl_to_stab.normalize();
    geometry_msgs::msg::TransformStamped s;
    s.header.stamp = stamp;
    s.header.frame_id = base_frame_;
    s.child_frame_id = stabilized_frame_;
    s.transform.rotation.x = q_bl_to_stab.x();
    s.transform.rotation.y = q_bl_to_stab.y();
    s.transform.rotation.z = q_bl_to_stab.z();
    s.transform.rotation.w = q_bl_to_stab.w();
    // translation stays zero: same origin, only de-tilted.
    broadcaster_->sendTransform(s);
  }

  std::string input_topic_;
  std::string odom_frame_;
  std::string base_frame_;
  bool lock_z_ {false};
  double fixed_z_ {0.0};
  bool publish_stabilized_ {true};
  std::string stabilized_frame_;

  // tilt source / IMU
  std::string tilt_source_;
  std::string imu_topic_;
  std::string imu_mode_;
  std::string resolved_mode_;
  double cf_tau_ {0.25};
  double roll_offset_ {0.0};
  double pitch_offset_ {0.0};
  bool imu_qos_reliable_ {false};
  bool use_imu_tilt_ {false};

  // complementary-filter state
  bool have_cf_state_ {false};
  double roll_ {0.0};
  double pitch_ {0.0};
  rclcpp::Time last_imu_time_;

  std::unique_ptr<tf2_ros::TransformBroadcaster> broadcaster_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr sub_;
  rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_sub_;
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<OdomTfBroadcaster>());
  rclcpp::shutdown();
  return 0;
}
