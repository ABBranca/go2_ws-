// scan_preprocessor — clean the XT16 cloud BEFORE the 2D flatten, staying strictly 2D.
//
// Rationale (see docs/wiki concepts/z-observability + slam-toolbox-2d): the XT16 is
// horizontal, so 3D SLAM is excluded and we flatten to /scan. pointcloud_to_laserscan
// collapses ALL 16 rings to one min-range-per-azimuth scan; rings far from horizontal
// (±15°) graze the floor/ceiling and inject spurious short ranges → ragged walls.
//
// This node keeps only the near-horizontal rings (default 7 & 8 ≈ ±1°, measured), whose
// returns strike vertical surfaces at a consistent horizontal range. It is a pure sensor-
// geometry filter — independent of any pose estimator (unlike attitude-leveling, which
// regressed on the noisy leg-odometry attitude). Per-point de-skew (using the `time`
// field + odom twist) is a future addition here when recordings have non-trivial motion.
//
//   /lidar_points ─> [scan_preprocessor: keep rings R_min..R_max] ─> /lidar_points_ring
//   /lidar_points_ring ─> pointcloud_to_laserscan ─> /scan ─> slam_toolbox

#include <cstring>
#include <memory>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>

class ScanPreprocessor : public rclcpp::Node {
public:
  ScanPreprocessor() : Node("scan_preprocessor") {
    input_topic_  = declare_parameter<std::string>("input_topic", "/lidar_points");
    output_topic_ = declare_parameter<std::string>("output_topic", "/lidar_points_ring");
    ring_field_   = declare_parameter<std::string>("ring_field", "ring");
    ring_min_     = declare_parameter<int>("ring_min", 7);   // ≈ +1.0° (measured)
    ring_max_     = declare_parameter<int>("ring_max", 8);   // ≈ -1.0° (measured)

    pub_ = create_publisher<sensor_msgs::msg::PointCloud2>(output_topic_, rclcpp::SensorDataQoS());
    sub_ = create_subscription<sensor_msgs::msg::PointCloud2>(
      input_topic_, rclcpp::SensorDataQoS(),
      [this](const sensor_msgs::msg::PointCloud2::SharedPtr msg) { on_cloud(*msg); });

    RCLCPP_INFO(get_logger(),
      "scan_preprocessor: %s -> %s  keep rings [%d..%d] on field '%s'",
      input_topic_.c_str(), output_topic_.c_str(), ring_min_, ring_max_, ring_field_.c_str());
  }

private:
  void on_cloud(const sensor_msgs::msg::PointCloud2 & in) {
    // Locate the ring field (uint16 expected for Hesai/Velodyne layouts).
    const sensor_msgs::msg::PointField * rf = nullptr;
    for (const auto & f : in.fields) {
      if (f.name == ring_field_) { rf = &f; break; }
    }
    if (rf == nullptr) {
      if (!warned_) {
        RCLCPP_WARN(get_logger(),
          "ring field '%s' not found — passing cloud through unfiltered", ring_field_.c_str());
        warned_ = true;
      }
      pub_->publish(in);
      return;
    }

    const size_t step = in.point_step;
    const size_t n = static_cast<size_t>(in.width) * in.height;

    sensor_msgs::msg::PointCloud2 out;
    out.header = in.header;
    out.fields = in.fields;
    out.is_bigendian = in.is_bigendian;
    out.point_step = in.point_step;
    out.is_dense = in.is_dense;
    out.height = 1;                                  // emit an unorganised cloud
    out.data.reserve(in.data.size());

    uint32_t kept = 0;
    for (size_t i = 0; i < n; ++i) {
      const size_t base = i * step;
      uint16_t ring;
      std::memcpy(&ring, in.data.data() + base + rf->offset, sizeof(ring));
      if (ring >= static_cast<uint16_t>(ring_min_) && ring <= static_cast<uint16_t>(ring_max_)) {
        out.data.insert(out.data.end(), in.data.begin() + base, in.data.begin() + base + step);
        ++kept;
      }
    }
    out.width = kept;
    out.row_step = static_cast<uint32_t>(out.data.size());
    pub_->publish(out);
  }

  std::string input_topic_, output_topic_, ring_field_;
  int ring_min_ {7}, ring_max_ {8};
  bool warned_ {false};
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_;
  rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr pub_;
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ScanPreprocessor>());
  rclcpp::shutdown();
  return 0;
}
